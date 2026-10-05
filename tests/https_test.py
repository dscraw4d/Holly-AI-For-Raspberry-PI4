"""Real TLS 1.2 interoperability; authentication failures must expose no body."""
import ctypes as C
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading
import time
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

ROOT=Path(__file__).resolve().parents[1]
now=datetime.now(timezone.utc)
def cert(key, name, issuer, issuer_key, ca=False, expired=False):
    start=now-timedelta(days=30 if expired else 1)
    end=now-timedelta(days=1) if expired else now+timedelta(days=30)
    return (x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,name)]))
            .issuer_name(issuer).public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(start).not_valid_after(end)
            .add_extension(x509.BasicConstraints(ca=ca,path_length=None),critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName(name)]),critical=False)
            .sign(issuer_key,hashes.SHA256()))

with tempfile.TemporaryDirectory(prefix='holly-https-') as tmp:
    tmp=Path(tmp);key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
    name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'Holly test CA')])
    ca=cert(key,'Holly test CA',name,key,ca=True)
    (tmp/'ca.pem').write_bytes(ca.public_bytes(serialization.Encoding.PEM))
    with (tmp/'https_test_anchors.h').open('w') as output:
        subprocess.run([str(ROOT/'vendor/bearssl/build/brssl'),'ta',str(tmp/'ca.pem')],stdout=output,stderr=subprocess.DEVNULL,check=True)
    subprocess.run(['cc','-shared','-fPIC','-O2','-Wall','-Wextra','-Werror','-DHOLLY_HTTPS_TEST_ANCHORS',
                    '-Isrc','-Ivendor/bearssl/inc','-I'+str(tmp),'src/https.c','src/web_format.c',
                    'tests/https_adapter.c','vendor/bearssl/build/libbearssl.a','-o',str(tmp/'test.so')],cwd=ROOT,check=True)
    lib=C.CDLL(str(tmp/'test.so'));io=C.CFUNCTYPE(C.c_int,C.POINTER(C.c_ubyte),C.c_uint,C.c_void_p)
    rng=C.CFUNCTYPE(C.c_int,C.POINTER(C.c_ubyte),C.c_size_t,C.c_void_p)
    lib.https_start.argtypes=[C.c_char_p,C.c_uint64,rng,io,io];lib.https_start_xml.argtypes=lib.https_start.argtypes;lib.https_start_ddg.argtypes=lib.https_start.argtypes;lib.https_body.restype=C.c_char_p
    for mode in ('valid','valid-xml','valid-ddg','wrong-name','expired','untrusted','rng-failure'):
        server_key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
        signer=key if mode!='untrusted' else server_key
        issued=cert(server_key,'elsewhere.invalid' if mode=='wrong-name' else 'en.wikipedia.org',name,signer,expired=mode=='expired')
        (tmp/'server.pem').write_bytes(issued.public_bytes(serialization.Encoding.PEM))
        (tmp/'key.pem').write_bytes(server_key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.minimum_version=context.maximum_version=ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(tmp/'server.pem',tmp/'key.pem')
        client,server=socket.socketpair();client.setblocking(False);server.settimeout(8)
        def serve():
            try:
                with context.wrap_socket(server,server_side=True) as s:
                    request=b''
                    while b'\r\n\r\n' not in request:request+=s.recv(4096)
                    body=json.dumps({'type':'standard','extract':'Red Dwarf is a British sitcom.'}).encode()
                    if mode=='valid-xml':body=b'<rss><channel><item><title>Red Dwarf fixture</title></item></channel></rss>'
                    response=b'HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: '+str(len(body)).encode()+b'\r\n\r\n'+body
                    if mode=='valid-xml':response=response.replace(b'application/json',b'text/xml')
                    if mode=='valid-ddg':response=response.replace(b'200 OK',b'202 Accepted').replace(b'application/json',b'application/x-javascript')
                    for i in range(0,len(response),7):s.sendall(response[i:i+7])
            except (ssl.SSLError,OSError):pass
        thread=threading.Thread(target=serve,daemon=True);thread.start()
        @io
        def write(p,n,_):
            try:return client.send(C.string_at(p,min(n,17)))
            except BlockingIOError:return 0
            except OSError:return -1
        @io
        def read(p,n,_):
            try:
                data=client.recv(min(n,11))
                if not data:return -1
                C.memmove(p,data,len(data));return len(data)
            except BlockingIOError:return 0
            except OSError:return -1
        @rng
        def random(p,n,_):
            if mode=='rng-failure':return -1
            C.memmove(p,os.urandom(n),n);return 0
        start=(lib.https_start_xml if mode=='valid-xml' else lib.https_start_ddg if mode=='valid-ddg' else lib.https_start)(b'en.wikipedia.org',int(now.timestamp()),random,write,read)
        result=start
        deadline=time.monotonic()+8
        if start==0:
            while time.monotonic()<deadline:
                result=lib.https_poll()
                if result:break
                time.sleep(.0005)
        if mode in ('valid','valid-xml','valid-ddg'):assert result==1 and lib.https_verified() and b'Red Dwarf' in lib.https_body(),(mode,result,lib.https_error())
        else:assert result==-1 and not lib.https_verified() and lib.https_body()==b'',(mode,result,lib.https_error())
        client.close();thread.join(timeout=1);lib.https_destroy()
        print('TLS:',mode,'passed')
