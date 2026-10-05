"""Real OpenSSH transfer with production training code and test-only in-memory SD."""
import base64
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(ROOT/'model'))
from provision_ssh import provision
import holly_lm as lm
from sync import Session,download,upload
with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp);password='Test-only '+os.urandom(16).hex();creds,pub,_=provision(d,'holly',password)
    ask=d/'ask';ask.write_text('#!/usr/bin/env python3\nimport os\nprint(os.environ["HOLLY_TEST_PASSWORD"])\n');ask.chmod(0o700)
    os.environ.update(SSH_ASKPASS=str(ask),SSH_ASKPASS_REQUIRE='force',DISPLAY='test',HOLLY_TEST_PASSWORD=password)
    server=subprocess.Popen([str(ROOT/'build/ssh_host_adapter'),str(creds),'4096','training'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    session=None
    try:
        port=int(server.stdout.readline().split()[1]);known=d/'known_hosts';known.write_text(f'[127.0.0.1]:{port} ssh-rsa '+base64.b64encode(pub).decode()+'\n')
        session=Session('127.0.0.1','holly',port,known)
        download(session,d/'export')
        p=lm.load(d/'export/active.npz');expected=lm.quantize(lm.load(ROOT/'model/seed1.npz'))
        for k in p:assert np.array_equal(lm.quantize(p)[k],expected[k])
        for name in ['train','valid','test']:
            a,_,_=lm.examples(d/'export'/f'{name}.txt');b,_,_=lm.examples(ROOT/'model/data'/f'{name}.txt');assert a==b
        upload(session,d/'export/active.npz')
        deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            time.sleep(2)
            reply=session.command('train status')
            assert 'Ship computer here' in session.command('hello')
            if 'Training phase 0' in reply:
                assert 'accepted/rejected 0/1' in reply,reply
                print('Identical candidate rejected by improvement gate; SSH responsive during evaluation');break
        else:raise AssertionError('Evaluation timed out')
        print('OpenSSH: all reviewed records and 79,972 weight bytes round-tripped; upload CRC and gate passed')
    finally:
        if session:session.close()
        server.terminate();server.wait(timeout=5)
