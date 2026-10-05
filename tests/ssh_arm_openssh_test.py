"""Real OpenSSH against the original server inside a bare-metal Pi 4 kernel.
QEMU lacks GENET/RNG200. Test-only UART framing supplies bytes and OS entropy,
not SSH/crypto/chat logic. This is not a Pi Ethernet hardware validation.
"""
import argparse
import base64
import os
from pathlib import Path
import selectors
import socket
import struct
import subprocess
import threading
import time
parser=argparse.ArgumentParser();parser.add_argument('--qemu',required=True);parser.add_argument('--credentials',type=Path,required=True)
args=parser.parse_args();ROOT=Path(__file__).resolve().parents[1]
pub=args.credentials.with_name('ssh-host-key.pub').read_text().split()[1]
password=args.credentials.with_name('test-password.txt').read_text().strip()
directory=args.credentials.parent
askpass=directory/'askpass';askpass.write_text('#!/usr/bin/python3\nimport os\nprint(os.environ["HOLLY_TEST_PASSWORD"])\n');askpass.chmod(0o700)
env=dict(os.environ,SSH_ASKPASS=str(askpass),SSH_ASKPASS_REQUIRE='force',DISPLAY='test',HOLLY_TEST_PASSWORD=password)
for fragment in [1,4096]:
    qemu=subprocess.Popen([args.qemu,'-M','raspi4b','-m','2G','-smp','4','-kernel',str(ROOT/'build/ssh-arm-test.img'),
        '-display','none','-serial','stdio','-monitor','none','-nic','none','-no-reboot'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();port=listener.getsockname()[1]
    errors=[];counts={'random_requests':0,'received':0}
    def bridge():
        conn=None;selector=selectors.DefaultSelector();selector.register(qemu.stdout,selectors.EVENT_READ)
        def read(n):
            result=bytearray();deadline=time.monotonic()+60
            while len(result)<n:
                if time.monotonic()>deadline:raise TimeoutError('ARM SSH did not respond')
                if selector.select(.1):
                    b=os.read(qemu.stdout.fileno(),n-len(result))
                    if not b:raise EOFError('QEMU stopped')
                    result.extend(b)
            return bytes(result)
        def command(kind,data=b''):
            qemu.stdin.write(kind+struct.pack('>I',len(data))+data);qemu.stdin.flush()
            while True:
                kind=read(1);n=struct.unpack('>I',read(4))[0]
                assert n<=35036
                if kind==b'R':
                    counts['random_requests']+=1;qemu.stdin.write(os.urandom(n));qemu.stdin.flush()
                elif kind==b'O':
                    output=read(n);counts['received']+=n;conn.sendall(output)
                elif kind==b'S':return n
                else:raise AssertionError(kind)
        try:
            assert read(8)==b'HLSSH23\n'
            conn,_=listener.accept();conn.settimeout(60);assert command(b'C')==0
            while True:
                incoming=conn.recv(fragment)
                if not incoming:break
                if command(b'I',incoming):break
        except BaseException as exc:errors.append(exc)
        finally:
            if conn:conn.close()
            selector.close()
    thread=threading.Thread(target=bridge,daemon=True);thread.start()
    known=directory/'known_hosts';known.write_text(f'[127.0.0.1]:{port} ssh-rsa {pub}\n')
    cmd=['ssh','-F','/dev/null','-T','-p',str(port),'-o','StrictHostKeyChecking=yes','-o',f'UserKnownHostsFile={known}',
        '-vv','-o','RekeyLimit=1K','-o','PreferredAuthentications=password','-o','PubkeyAuthentication=no','holly@127.0.0.1']
    try:
        result=subprocess.run(cmd,input='personality off\nchat model 1\nchat on\nhow are you\nthanks holly\nchat off\nhello\nteach name of ship => Red Dwarf\nask name of ship\n'+'hello\n'*40+'repeat that\nexit\n',env=env,capture_output=True,text=True,timeout=90)
        thread.join(5);assert not errors,errors
        assert result.returncode==0,(result.stdout,result.stderr)
        assert 'running and ready to chat' in result.stdout and 'You are welcome' in result.stdout,result.stdout
        assert 'Ship computer here' in result.stdout and 'Holly: Red Dwarf' in result.stdout,result.stdout
        assert result.stdout.count('Holly: Red Dwarf')==2,result.stdout
        cycles=result.stderr.count('SSH2_MSG_NEWKEYS received')
        assert cycles>=2,result.stderr[-3000:]
        assert not thread.is_alive()
        print(f'AArch64 Pi 4 kernel SSH: fragment={fragment}: {counts}; {cycles} key exchanges, encrypted chat and retained context passed')
    finally:
        listener.close();qemu.terminate();qemu.wait(timeout=5)
