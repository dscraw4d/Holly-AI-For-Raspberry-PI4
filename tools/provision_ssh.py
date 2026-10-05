#!/usr/bin/env python3
"""Create a unique SSH identity on the build host; no crypto dependency in OS.
Never distribute credentials. Password is read interactively, not in argv.
"""
import argparse
import base64
import getpass
import hashlib
import os
from pathlib import Path
import struct
import ipaddress
from cryptography.hazmat.primitives.asymmetric import rsa

def ssh_string(b): return struct.pack('>I',len(b))+b
def mpint(n):
    b=n.to_bytes((n.bit_length()+7)//8,'big')
    if b and b[0]&128:b=b'\0'+b
    return ssh_string(b)
def provision(directory, username, password, ip='169.254.77.1', allow_public_demo=False):
    if not 1<=len(username.encode('ascii'))<=31:raise ValueError('Username: 1 to 31 ASCII characters')
    minimum=8 if allow_public_demo else 12
    if not minimum<=len(password.encode())<=256:raise ValueError(f'Password: {minimum} to 256 UTF-8 bytes')
    if allow_public_demo and (username,password)!=('Rimmer','smeghead'):
        raise ValueError('The demo exception applies only to the requested Rimmer/smeghead account')
    directory.mkdir(parents=True,exist_ok=True)
    path=directory/'ssh-credentials.bin'
    if path.exists():raise FileExistsError('Credentials already exist; refusing replacement')
    key=rsa.generate_private_key(public_exponent=65537,key_size=2048).private_numbers()
    salt=os.urandom(32)
    data=(b'HLYSSH23'+key.public_numbers.n.to_bytes(256,'big')+key.d.to_bytes(256,'big')+
          salt+hashlib.sha256(salt+password.encode()).digest()+username.encode().ljust(33,b'\0'))
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'wb') as f:f.write(data)
    pub=ssh_string(b'ssh-rsa')+mpint(65537)+mpint(key.public_numbers.n)
    (directory/'ssh-host-key.pub').write_text('ssh-rsa '+base64.b64encode(pub).decode()+' holly\n')
    fingerprint='SHA256:'+base64.b64encode(hashlib.sha256(pub).digest()).decode().rstrip('=')
    (directory/'ssh-host-fingerprint.txt').write_text(fingerprint+'\n')
    def c_array(b):return '{'+','.join(f'0x{x:02x}' for x in b)+'}'
    ip_bytes=ipaddress.IPv4Address(ip).packed
    header=('#ifndef HOLLY_GENERATED_CREDENTIALS_H\n#define HOLLY_GENERATED_CREDENTIALS_H\n'
            '#include "ssh_server.h"\nstatic const struct holly_ssh_credentials holly_credentials={\n'+
            ',\n'.join(c_array(b) for b in [data[8:264],data[264:520],salt,data[552:584],data[584:]])+
            '\n};\nstatic const uint8_t holly_network_ip[4]='+c_array(ip_bytes)+';\n#endif\n')
    header_path=directory/'ssh_credentials_generated.h'
    fd=os.open(header_path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w') as f:f.write(header)
    return path,pub,fingerprint

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path);parser.add_argument('--username',default='holly')
    parser.add_argument('--ip',default='169.254.77.1')
    parser.add_argument('--public-demo',action='store_true',help='Explicitly allow Rimmer/smeghead for a LAN demo image')
    args=parser.parse_args();password=getpass.getpass('Holly SSH password (at least 12 characters): ')
    if password!=getpass.getpass('Repeat password: '):raise SystemExit('Passwords differ')
    path,_,fingerprint=provision(args.directory,args.username,password,args.ip,allow_public_demo=args.public_demo)
    print(f'Created {path}\nHost fingerprint: {fingerprint}')
