#!/usr/bin/env python3
"""Upload text to Holly v0.40+ through the installed OpenSSH client. No pip packages."""
import argparse
import hashlib
from pathlib import Path
import queue
import re
import subprocess
import threading
import unicodedata

LIMIT=32*1024*1024
class HollySSH:
    def __init__(self,host,user='holly',port=22,options=(),timeout=120):
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]*',host) or not re.fullmatch(r'[A-Za-z0-9_]+',user):
            raise ValueError('Use an IPv4 address or simple host name and user name')
        self.timeout=timeout
        args=['ssh','-T','-p',str(port),'-o','RekeyLimit=4M','-o','ConnectTimeout=60']
        for option in options:args+=['-o',option]
        args += [f'{user}@{host}']
        self.process=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
        self.incoming=queue.Queue()
        def read():
            while True:
                b=self.process.stdout.read(1)
                self.incoming.put(b)
                if not b:break
        threading.Thread(target=read,daemon=True).start()
        self.prompt()
    def prompt(self):
        data=bytearray()
        while not data.endswith(b'holly> '):
            try:b=self.incoming.get(timeout=self.timeout)
            except queue.Empty:raise RuntimeError('Timed out waiting for Holly; rerun to resume')
            if not b:raise RuntimeError('SSH disconnected; rerun to resume')
            data.extend(b)
            if len(data)>1024*1024:raise RuntimeError('Unexpectedly large SSH response')
        return data.decode('ascii',errors='replace')[:-7]
    def command(self,text):
        self.process.stdin.write(text.encode('ascii')+b'\n');self.process.stdin.flush()
        return self.prompt()
    def batch(self,commands):
        self.process.stdin.write(('\n'.join(commands)+'\n').encode('ascii'))
        self.process.stdin.flush()
        return [self.prompt() for _ in commands]
    def close(self):
        if self.process.poll() is None:
            try:self.process.stdin.write(b'exit\n');self.process.stdin.flush();self.process.wait(timeout=3)
            except (OSError,subprocess.TimeoutExpired):self.process.terminate()
        self.process.stdout.close();self.process.stdin.close()

def prepare(path,normalize=False):
    text=Path(path).read_text(encoding='utf-8-sig').replace('\r\n','\n').replace('\r','\n')
    if normalize:
        text=text.translate(str.maketrans({'\u2018':"'",'\u2019':"'",'\u201c':'"','\u201d':'"','\u2013':'-','\u2014':'--','\u2026':'...'}))
        text=unicodedata.normalize('NFKD',text).encode('ascii','replace').decode('ascii')
    try:data=text.encode('ascii')
    except UnicodeEncodeError:raise ValueError('Text contains non-ASCII characters. Use --normalize to transliterate a copy; original file is unchanged.')
    if not data or any(c<32 and c not in (9,10) or c>126 for c in data):raise ValueError('Use nonempty plain text without terminal control characters')
    
    return data

def capabilities(client):
    if hasattr(client,'document_capabilities'):return client.document_capabilities
    status=client.command('doc status')
    if 'DOC bank online' not in status:raise RuntimeError('SSH to Holly and run doc format once for v0.49.15, then exit.\n'+status.strip())
    fields={k:int(v) for k,v in re.findall(r'(slots|bytes|chunk)=(\d+)',status)}
    cap=(fields.get('slots',16),fields.get('bytes',131072),fields.get('chunk',128))
    if not 1<=cap[0]<=4096 or not 1<=cap[1]<=1024*1024*1024 or cap[2] not in (128,4096):raise RuntimeError('Invalid server document capabilities')
    client.document_capabilities=cap
    return cap

def upload(client,data,title,progress=print):
    if not 1<=len(title)<=80 or not all(32<=ord(c)<127 for c in title):raise ValueError('Title must be 1-80 printable ASCII characters')
    slots,limit,chunk_bytes=capabilities(client)
    parts=[data[i:i+limit] for i in range(0,len(data),limit)]
    if not parts:raise ValueError('Document is empty')
    if len(parts)>slots:raise ValueError('Document needs more slots than the bank provides')
    for part,body in enumerate(parts,1):
        name=title if len(parts)==1 else title[:60]+f' [part {part}/{len(parts)}]'
        digest=hashlib.sha256(body).hexdigest()
        reply=client.command(f'doc begin {len(body)} {digest} {name}')
        match=re.search(r'^DOC READY (\d+) (\d+)\r?$',reply,re.M)
        if not match:raise RuntimeError(reply.strip())
        doc,offset=map(int,match.groups())
        if offset>len(body) or offset%chunk_bytes and offset!=len(body):raise RuntimeError('Invalid resume offset')
        progress(f'{name}: document {doc}, resuming at {offset}/{len(body)} bytes')
        while offset<len(body):
            commands=[];expected=[]
            # Four durable pages in flight; no acknowledgement per 128 bytes.
            for _ in range(4 if chunk_bytes==4096 and hasattr(client,'batch') else 1):
                if offset>=len(body):break
                chunk=body[offset:offset+chunk_bytes]
                commands.append(f'doc put {doc} {offset} {chunk.hex()}');offset+=len(chunk);expected.append(offset)
            replies=client.batch(commands) if len(commands)>1 else [client.command(commands[0])]
            for reply,end in zip(replies,expected):
                if not re.search(rf'^DOC ACK {end}\r?$',reply,re.M):raise RuntimeError(reply.strip())
            if offset%65536==0 or offset==len(body):progress(f'  {offset}/{len(body)} bytes verified')
        reply=client.command(f'doc commit {doc}')
        if not re.search(r'^DOC COMMITTED\r?$',reply,re.M):raise RuntimeError(reply.strip())
        progress(f'Committed document {doc}; SHA256 {digest}')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('host');p.add_argument('file',type=Path);p.add_argument('--title');p.add_argument('--user',default='Rimmer');p.add_argument('--port',type=int,default=22)
    p.add_argument('--normalize',action='store_true',help='Transliterate Unicode into ASCII before upload')
    p.add_argument('--ssh-option',action='append',default=[],help='Additional OpenSSH option, e.g. UserKnownHostsFile=path')
    a=p.parse_args();client=None
    try:
        data=prepare(a.file,a.normalize)
        print(f'Prepared {len(data)} bytes. Training is not performed. Enter your SSH password when prompted.')
        client=HollySSH(a.host,a.user,a.port,a.ssh_option)
        status=client.command('doc status')
        if 'DOC bank online' not in status:raise RuntimeError('Initialize the bank first: SSH to Holly, run doc format, then exit.\n'+status.strip())
        upload(client,data,a.title or a.file.stem)
        print('Done. Ask Holly: find <phrase>. Keep your original files as backups.')
    except (ValueError,RuntimeError,OSError,KeyboardInterrupt) as e:
        p.exit(1,f'Upload stopped: {e}\nRerun the same upload to resume verified chunks.\n')
    finally:
        if client:client.close()
if __name__=='__main__':main()
