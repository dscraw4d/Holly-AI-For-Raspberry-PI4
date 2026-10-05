"""Run the actual TLS client on AArch64 with a test-only UART transport."""
import argparse
from datetime import datetime,timedelta,timezone
import os
from pathlib import Path
import selectors
import socket
import ssl
import struct
import subprocess
import threading
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

parser=argparse.ArgumentParser();parser.add_argument('--qemu',required=True);args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[1];directory=ROOT/'build/https-test';directory.mkdir(exist_ok=True)
now=datetime.now(timezone.utc);key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'Holly AArch64 test CA')])
def certificate(subject,public,ca):
    return (x509.CertificateBuilder().subject_name(subject).issuer_name(name).public_key(public).serial_number(x509.random_serial_number())
            .not_valid_before(now-timedelta(days=1)).not_valid_after(now+timedelta(days=30))
            .add_extension(x509.BasicConstraints(ca=ca,path_length=None),critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName('en.wikipedia.org')]),critical=False).sign(key,hashes.SHA256()))
ca=certificate(name,key.public_key(),True);server_key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
server_cert=certificate(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'en.wikipedia.org')]),server_key.public_key(),False)
(directory/'ca.pem').write_bytes(ca.public_bytes(serialization.Encoding.PEM));(directory/'server.pem').write_bytes(server_cert.public_bytes(serialization.Encoding.PEM))
(directory/'key.pem').write_bytes(server_key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
with (directory/'https_test_anchors.h').open('w') as output:
    subprocess.run([str(ROOT/'vendor/bearssl/build/brssl'),'ta',str(directory/'ca.pem')],stdout=output,stderr=subprocess.DEVNULL,check=True)
with (directory/'build.log').open('w') as output:
    subprocess.run(['make','build/https-arm-test.img'],cwd=ROOT,stdout=output,stderr=subprocess.STDOUT,check=True)
for expired in (False,True):
    context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.minimum_version=context.maximum_version=ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(directory/'server.pem',directory/'key.pem')
    raw,server=socket.socketpair();raw.settimeout(.005);server.settimeout(60)
    def serve():
        try:
            with context.wrap_socket(server,server_side=True) as s:
                request=b''
                while b'\r\n\r\n' not in request:request+=s.recv(4096)
                body=b'{"type":"standard","extract":"Holly is reading through verified TLS."}'
                s.sendall(b'HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: '+str(len(body)).encode()+b'\r\n\r\n'+body)
        except (ssl.SSLError,OSError):pass
    thread=threading.Thread(target=serve,daemon=True);thread.start()
    qemu=subprocess.Popen([args.qemu,'-M','raspi4b','-m','2G','-smp','4','-kernel',str(ROOT/'build/https-arm-test.img'),'-display','none','-serial','stdio','-monitor','none','-nic','none','-no-reboot'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    selector=selectors.DefaultSelector();selector.register(qemu.stdout,selectors.EVENT_READ)
    def read(n):
        result=bytearray()
        while len(result)<n:
            if not selector.select(60):raise TimeoutError('AArch64 TLS stalled')
            data=os.read(qemu.stdout.fileno(),n-len(result))
            if not data:raise RuntimeError('QEMU stopped')
            result.extend(data)
        return bytes(result)
    try:
        assert read(8)==b'HLHTTPS\n'
        epoch=int((now+timedelta(days=40) if expired else now).timestamp());qemu.stdin.write(struct.pack('>Q',epoch));qemu.stdin.flush()
        body=b'';error=0
        while True:
            kind=read(1);n=struct.unpack('>I',read(4))[0]
            if kind==b'R':qemu.stdin.write(os.urandom(n));qemu.stdin.flush()
            elif kind==b'O':raw.sendall(read(n))
            elif kind==b'Q':
                try:data=raw.recv(n)
                except socket.timeout:data=None
                if data is None:qemu.stdin.write(struct.pack('>I',0))
                elif not data:qemu.stdin.write(struct.pack('>I',0xffffffff))
                else:qemu.stdin.write(struct.pack('>I',len(data))+data)
                qemu.stdin.flush()
            elif kind==b'B':body=read(n)
            elif kind==b'E':error=n
            elif kind==b'S':
                if expired:assert n==2 and not body and error
                else:assert n==1 and b'verified TLS' in body and not error
                break
            else:raise AssertionError(kind)
        print('AArch64 verified TLS:', 'expired certificate rejected without content' if expired else 'encrypted HTTP response accepted')
    finally:
        raw.close();qemu.terminate();qemu.wait(timeout=5);selector.close();thread.join(timeout=1)
