"""Transfer reviewed examples and candidate weights over one existing OpenSSH session.
Requires Holly v0.35; checks the existing known_hosts entry. Passwords are handled
by OpenSSH's terminal/askpass, never by this script. No Python SSH package needed.
"""
import argparse
import json
from pathlib import Path
import queue
import re
import subprocess
import threading
import time
import zlib
import numpy as np
import holly_lm as lm

class Session:
    def __init__(self,host,user='holly',port=22,known_hosts=None):
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.:-]*',host) or not re.fullmatch(r'[A-Za-z0-9_-]+',user):
            raise ValueError('Use a hostname/IP and a separate username')
        options=['-o','UserKnownHostsFile='+str(known_hosts)] if known_hosts else []
        self.p=subprocess.Popen(['ssh','-F','/dev/null' if __import__('os').name!='nt' else 'NUL','-T',
            '-o','StrictHostKeyChecking=yes','-o','ConnectTimeout=10','-o','ServerAliveInterval=10',
            '-p',str(port),'-l',user]+options+[host],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
        self.q=queue.Queue()
        def reader():
            while True:
                b=self.p.stdout.read(1);self.q.put(b)
                if not b:break
        threading.Thread(target=reader,daemon=True).start()
        try:self.read_prompt()
        except BaseException:
            self.close();raise
    def read_prompt(self,timeout=90):
        data=bytearray();deadline=time.monotonic()+timeout
        while not data.endswith(b'holly> '):
            remaining=deadline-time.monotonic()
            if remaining<=0:raise TimeoutError('Holly did not return a prompt')
            b=self.q.get(timeout=remaining)
            if not b:raise RuntimeError('SSH closed before reply')
            data.extend(b)
            if len(data)>16384:raise RuntimeError('Oversized reply')
        return data.decode('ascii',errors='strict').replace('\r','')
    def command(self,line):
        if len(line)>310 or '\n' in line:raise ValueError('Invalid command')
        self.p.stdin.write(line.encode('ascii')+b'\n');self.p.stdin.flush();return self.read_prompt()
    def saved(self):
        deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            reply=self.command('train status')
            if 'SAVE FAILED' in reply or 'RAM ONLY' in reply:raise RuntimeError(reply)
            if '; saved\n' in reply:return reply
            time.sleep(.1)
        raise TimeoutError('Checkpoint did not finish')
    def control(self,line):
        for _ in range(3):
            reply=self.command(line)
            if 'BUSY saving' in reply:self.saved();continue
            if 'OK ' not in reply:raise RuntimeError(reply)
            return reply
        raise RuntimeError('Checkpoint remains busy')
    def close(self):
        if self.p.poll() is None:
            try:self.p.stdin.write(b'exit\n');self.p.stdin.flush();self.p.wait(timeout=5)
            except (OSError,subprocess.TimeoutExpired):self.p.kill();self.p.wait()

def download(session,directory):
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
    if any(directory.iterdir()):raise ValueError('Use an empty export directory')
    session.control('train pause');status=session.saved()
    count=int(re.search(r'; records (\d+)',status).group(1));records=[]
    if not 54<=count<=128:raise ValueError('Unexpected dataset size')
    for i in range(count):
        reply=session.command(f'train export {i}')
        match=re.search(r'^EXAMPLE ([012]) (\d+) (\d+) ([0-9a-f]+) ([0-9a-f]+)$',reply,re.M)
        if not match:raise RuntimeError('Invalid example reply')
        split,mid,revision,source,text=match.groups()
        records.append({'split':int(split),'memory_id':int(mid),'revision':int(revision),
                        'source':bytes.fromhex(source).decode('ascii'),'text':bytes.fromhex(text).decode('ascii')})
    data=bytearray()
    for off in range(0,39986*2,128):
        reply=session.command(f'model export {off}')
        match=re.search(r'^WEIGHTS (\d+) ([0-9a-f]+)$',reply,re.M)
        if not match or int(match[1])!=off:raise RuntimeError('Invalid model chunk')
        chunk=bytes.fromhex(match[2]);expected=min(128,39986*2-off)
        if len(chunk)!=expected:raise RuntimeError('Model chunk length')
        data.extend(chunk)
    flat=np.frombuffer(data,dtype='<i2').astype(float)/lm.SCALE;params={};off=0
    for k,shape in lm.SHAPES.items():
        n=int(np.prod(shape));params[k]=flat[off:off+n].reshape(shape);off+=n
    np.savez(directory/'active.npz',**params)
    for split,name in enumerate(['train','valid','test']):
        (directory/(name+'.txt')).write_text('\n\n'.join(r['text'] for r in records if r['split']==split)+'\n',encoding='utf-8')
    (directory/'provenance.json').write_text(json.dumps(records,indent=2)+'\n')
    print('Exported reviewed examples and active weights. Holly remains paused; use train resume when ready.')

def upload(session,checkpoint):
    q=lm.quantize(lm.load(checkpoint))
    data=b''.join(q[k].astype('<i2').tobytes() for k in lm.SHAPES)
    session.control('train cancel');session.saved();session.control('model import begin')
    for off in range(0,len(data),128):
        reply=session.command(f'model import {off} {data[off:off+128].hex()}')
        if 'OK chunk' not in reply:raise RuntimeError(reply)
    reply=session.command(f'model import finish {zlib.crc32(data)}')
    if 'OK candidate queued for evaluation' not in reply:raise RuntimeError(reply)
    session.saved()
    print('Uploaded candidate. Holly will evaluate it while idle and activate it only if all gates pass. Use train status.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--host',required=True);p.add_argument('--user',default='holly');p.add_argument('--port',type=int,default=22);p.add_argument('--known-hosts')
    s=p.add_subparsers(dest='action',required=True);s.add_parser('download').add_argument('directory');s.add_parser('upload').add_argument('checkpoint')
    args=p.parse_args();session=None
    try:
        session=Session(args.host,args.user,args.port,args.known_hosts)
        if args.action=='download':download(session,args.directory)
        else:upload(session,args.checkpoint)
    finally:
        if session:session.close()
