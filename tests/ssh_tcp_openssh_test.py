"""OpenSSH through Holly's actual Ethernet/IP/TCP/SSH stack, with a synthetic
Ethernet peer. Host sockets carry client bytes only, not Holly TCP state.
Includes dropped SYN-ACK/data, duplicates, checksum rejection and seq wrap.
"""
import base64
import ctypes as C
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from provision_ssh import provision
lib=C.CDLL(str(ROOT/'build/tcp_stream_adapter.so'))
def checksum(p):
    if len(p)%2:p+=b'\0'
    value=sum(struct.unpack('>'+str(len(p)//2)+'H',p))
    while value>>16:value=(value&65535)+(value>>16)
    return ~value&65535
MAC=b'\x02\x01\x02\x03\x04\x05';PEER=b'\x02\x07\x08\x09\x0a\x0b'
IP=bytes([169,254,77,1]);PEERIP=bytes([169,254,77,2])
def packet(seq,ack,flags,data=b'',window=32768,options=b''):
    tcp=bytearray(struct.pack('>HHIIBBHHH',50000,22,seq,ack,(5+len(options)//4)<<4,flags,window,0,0)+options+data)
    tcp[16:18]=struct.pack('>H',checksum(PEERIP+IP+b'\0\x06'+struct.pack('>H',len(tcp))+tcp))
    ip=bytearray(struct.pack('>BBHHHBBH4s4s',0x45,0,20+len(tcp),0,0x4000,64,6,0,PEERIP,IP))
    ip[10:12]=struct.pack('>H',checksum(ip))
    return MAC+PEER+b'\x08\x00'+ip+tcp
def feed(f):return lib.test_tcp_feed(C.create_string_buffer(f),len(f))
def pop():
    buf=(C.c_ubyte*1514)();n=lib.test_tcp_pop(buf);return bytes(buf[:n]) if n else None

with tempfile.TemporaryDirectory(prefix='holly-tcp-') as tmp:
    directory=Path(tmp);password='Holly TCP test '+os.urandom(12).hex();creds,pub,_=provision(directory,'holly',password)
    askpass=directory/'askpass';askpass.write_text('#!/usr/bin/python3\nimport os\nprint(os.environ["HOLLY_TEST_PASSWORD"])\n');askpass.chmod(0o700)
    env=dict(os.environ,SSH_ASKPASS=str(askpass),SSH_ASKPASS_REQUIRE='force',DISPLAY='test',HOLLY_TEST_PASSWORD=password)
    # An advertised MSS is a real limit, even when it is below 64 bytes.
    credentials_data=creds.read_bytes()
    data=creds.read_bytes()
    assert lib.test_tcp_start(C.create_string_buffer(data),len(data))==0
    lib.test_ack_order_start()
    assert feed(packet(700,0,2))==0
    synack=pop();ack=(struct.unpack_from('>I',synack,38)[0]+1)&0xffffffff
    assert feed(packet(701,ack,16))==0
    assert feed(packet(701,ack,24,b'x'))==0 and lib.test_ack_order_seen()==1
    print('TCP ACK is transmitted before application/key-exchange processing passed')
    # Port probe closes before reading the banner, then immediately reconnects.
    data=creds.read_bytes()
    for trial in range(3):
        assert lib.test_tcp_start(C.create_string_buffer(data),len(data))==0
        assert feed(packet(400,0,2))==0
        synack=pop();ack=(struct.unpack_from('>I',synack,38)[0]+1)&0xffffffff
        assert feed(packet(401,ack,16))==0 and pop() is not None
        assert feed(packet(401,ack,17))==0 and lib.test_tcp_phase()==0
        while pop() is not None:pass
        assert feed(packet(500,0,2))==0 and pop()[47]==0x12
        assert lib.test_tcp_tick(10000)==0 and lib.test_tcp_phase()==0
    assert lib.test_tcp_start(C.create_string_buffer(data),len(data))==0
    assert feed(packet(600,0,2))==0
    synack=pop();ack=(struct.unpack_from('>I',synack,38)[0]+1)&0xffffffff
    assert feed(packet(601,ack,16))==0 and pop() is not None
    assert lib.test_tcp_tick(15000)==0 and lib.test_tcp_phase()==0
    print('Port probe close, immediate reconnect, SYN timeout and silent SSH timeout passed')
    for mss in [0,1,32,1460]:
        assert lib.test_tcp_start(C.create_string_buffer(credentials_data),len(credentials_data))==0
        syn=packet(100,0,2,options=b'\x02\x04'+struct.pack('>H',mss))
        if not mss:assert feed(syn)==-1;continue
        assert feed(syn)==0;reply=pop();ack=(struct.unpack_from('>I',reply,38)[0]+1)&0xffffffff
        assert feed(packet(101,ack,16))==0;banner=pop()
        assert len(banner)-54<=mss
        # Out-of-window reset must not tear down the established stream.
        assert feed(packet(9999,ack,4))==0 and lib.test_tcp_phase()==2
    print('Holly TCP: zero/small MSS validation and out-of-window reset rejection passed')
    assert lib.test_tcp_start(C.create_string_buffer(credentials_data),len(credentials_data))==0
    assert feed(packet(200,0,2,options=b'\x02\x04\x05\xb4'))==0
    reply=pop();ack=(struct.unpack_from('>I',reply,38)[0]+1)&0xffffffff
    assert feed(packet(201,ack,16,window=0))==0 and pop() is None
    assert lib.test_tcp_tick(999)==0 and pop() is None
    assert lib.test_tcp_tick(1)==0;probe=pop();assert len(probe)==55
    assert feed(packet(201,ack,16,window=0))==0 and pop() is None
    assert lib.test_tcp_tick(1000)==0;assert pop()==probe
    for i in range(12):
        assert feed(packet(201,ack,16,window=0))==0
        assert lib.test_tcp_tick(8000)==0
        assert pop()==probe and lib.test_tcp_phase()==2
    # Reopening the window acknowledges just the probe, not the whole queue.
    assert feed(packet(201,(ack+1)&0xffffffff,16,window=32768))==0
    rest=pop();assert rest and len(rest)>55
    lib.test_tcp_link_lost();assert lib.test_tcp_phase()==0
    assert feed(packet(300,0,2))==0 and pop()[47]==0x12
    print('Holly TCP: zero-window persist/recovery and immediate link-loss reconnect passed')
    for lose in [False,True]:
        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();port=listener.getsockname()[1]
        errors=[];stats={'duplicates':0,'lost':0,'received':0}
        def bridge():
            conn=None
            try:
                conn,_=listener.accept();conn.settimeout(0.1)
                data=creds.read_bytes();assert lib.test_tcp_start(C.create_string_buffer(data),len(data))==0
                seq=0xfffff000;ack=0
                syn=packet(seq,ack,2,options=b'\x02\x04\x01\x00');assert feed(syn)==0;seq=(seq+1)&0xffffffff
                synack=pop();assert synack and synack[47]==0x12
                if lose:
                    assert lib.test_tcp_tick(1000)==0;again=pop();assert again==synack;stats['lost']+=1
                ack=(struct.unpack_from('>I',synack,38)[0]+1)&0xffffffff
                assert feed(packet(seq,ack,16,window=15))==0
                first=True;closed=False;last_tick=time.monotonic()
                def drain():
                    nonlocal ack,first,closed
                    while True:
                        f=pop()
                        if not f:break
                        ihl=(f[14]&15)*4;t=f[14+ihl:14+struct.unpack_from('>H',f,16)[0]];header=(t[12]>>4)*4
                        remote=struct.unpack_from('>I',t,4)[0];flags=t[13];payload=t[header:]
                        assert checksum(f[14:14+ihl])==0
                        assert checksum(IP+PEERIP+b'\0\x06'+struct.pack('>H',len(t))+t)==0
                        if payload:
                            if lose and first:first=False;stats['lost']+=1;lib.test_tcp_tick(1000);continue
                            first=False
                            if remote==ack:conn.sendall(payload);ack=(ack+len(payload))&0xffffffff;stats['received']+=len(payload)
                            # Immediate ACK drives the one-segment transmit window.
                            assert feed(packet(seq,ack,16))==0
                        if flags&1:
                            if remote==ack:ack=(ack+1)&0xffffffff
                            feed(packet(seq,ack,17));closed=True
                drain()
                while not closed:
                    try:incoming=conn.recv(4096)
                    except socket.timeout:incoming=None
                    if incoming==b'':
                        feed(packet(seq,ack,17));drain();break
                    if incoming:
                        for start in range(0,len(incoming),97):
                            chunk=incoming[start:start+97];f=packet(seq,ack,24,chunk)
                            # Invalid TCP checksum must never reach SSH.
                            bad=bytearray(f);bad[-1]^=1;assert feed(bytes(bad))==-1
                            assert feed(f)==0
                            if lose:assert feed(f)==0;stats['duplicates']+=1
                            seq=(seq+len(chunk))&0xffffffff;drain()
                    now=time.monotonic();elapsed=int((now-last_tick)*1000)
                    if elapsed:assert lib.test_tcp_tick(elapsed)==0;last_tick=now
                    drain()
            except BaseException as exc:errors.append(exc)
            finally:
                if conn:conn.close()
        thread=threading.Thread(target=bridge,daemon=True);thread.start()
        known=directory/'known';known.write_text(f'[127.0.0.1]:{port} ssh-rsa '+base64.b64encode(pub).decode()+'\n')
        cmd=['ssh','-F','/dev/null','-T','-p',str(port),'-o','StrictHostKeyChecking=yes','-o',f'UserKnownHostsFile={known}',
             '-vv','-o','RekeyLimit=1K','-o','PreferredAuthentications=password','-o','PubkeyAuthentication=no','holly@127.0.0.1']
        result=subprocess.run(cmd,input='personality off\nhello\nteach our ship => Red Dwarf\nask our ship\n'+'hello\n'*40+'repeat that\nexit\n',env=env,capture_output=True,text=True,timeout=30)
        thread.join(5);listener.close();assert not errors,errors
        assert result.returncode==0,(result.stdout,result.stderr)
        assert 'Ship computer here' in result.stdout and 'Holly: Red Dwarf' in result.stdout,result.stdout
        assert result.stdout.count('Holly: Red Dwarf')==2,result.stdout
        cycles=result.stderr.count('SSH2_MSG_NEWKEYS received');assert cycles>=2,result.stderr
        assert not thread.is_alive()
        print(f'OpenSSH over Holly TCP: loss={lose}: {stats}; {cycles} key exchanges, encrypted login/chat and retained context passed')
