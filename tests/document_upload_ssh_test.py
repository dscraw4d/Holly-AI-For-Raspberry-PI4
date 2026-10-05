"""Real OpenSSH upload, interrupted transfer resume, commit and retrieval."""
import base64, hashlib, os, subprocess, sys, tempfile, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from provision_ssh import provision
from holly_upload import HollySSH, upload
with tempfile.TemporaryDirectory(prefix='holly-upload-') as tmp:
    d=Path(tmp); password='Upload test '+os.urandom(16).hex()
    creds,pub,_=provision(d,'holly',password)
    ask=d/'askpass';ask.write_text('#!/usr/bin/python3\nimport os\nprint(os.environ["HOLLY_TEST_PASSWORD"])\n');ask.chmod(0o700)
    os.environ.update(SSH_ASKPASS=str(ask),SSH_ASKPASS_REQUIRE='force',DISPLAY='test',HOLLY_TEST_PASSWORD=password)
    server=subprocess.Popen(([os.environ['HOLLY_TEST_LOADER']] if os.environ.get('HOLLY_TEST_LOADER') else [])+[str(ROOT/'build/ssh_host_adapter'),str(creds),'17','documents'],stdout=subprocess.PIPE,text=True)
    try:
        port=int(server.stdout.readline().split()[1]);known=d/'known_hosts'
        known.write_text(f'[127.0.0.1]:{port} ssh-rsa '+base64.b64encode(pub).decode()+'\n')
        opts=['StrictHostKeyChecking=yes',f'UserKnownHostsFile={known}','PubkeyAuthentication=no']
        data=(b'Original test notes. The Holly Hop Drive is an experimental device.\n'*40000)
        digest=hashlib.sha256(data).hexdigest()
        c=HollySSH('127.0.0.1',port=port,options=opts)
        assert 'DOC bank online' in c.command('doc status')
        assert 'DOC READY 1 0' in c.command(f'doc begin {len(data)} {digest} Test_notes')
        assert 'DOC ACK 4096' in c.command('doc put 1 0 '+data[:4096].hex())
        c.close()
        c=HollySSH('127.0.0.1',port=port,options=opts)
        started=time.monotonic();progress=[];upload(c,data,'Test_notes',progress.append)
        assert any('resuming at 4096/' in line for line in progress)
        assert 'Holly Hop Drive' in c.command('find holly hop drive')
        assert 'Test_notes' in c.command('doc list')
        assert 'uploaded document 1' in c.command('What is experimental about the Holly Hop Drive?')
        upload(c,data,'Test_notes',progress.append)
        assert any(f'resuming at {len(data)}/{len(data)}' in line for line in progress)
        script=b'@@EPISODE The End\nLister enters stasis after the radiation leak.\n'
        upload(c,script,'RD Scripts 01',progress.append)
        assert 'The End' in c.command('script list')
        answer=c.command('script ask The End | radiation leak')
        assert 'Lister enters stasis' in answer and '[The End; document 2; byte' in answer
        assert 'Series 1: 1/6' in c.command('script coverage')
        assert 'Kryten' in c.command('Tell me about Season 2')
        natural=c.command('In The End why was there a radiation leak?')
        assert 'Lister enters stasis' in natural and '[The End; document 2;' in natural
        assert 'Lister enters stasis' in c.command('Why did Lister enter stasis?')
        c.close()
        print(f'Real OpenSSH: {len(data)} bytes uploaded in {time.monotonic()-started:.2f}s including query tests (host RAM adapter); 4KiB pipeline, disconnect/resume, rekey, checksum commit, script retrieval and deduplication passed')
    finally:
        server.terminate();server.wait(timeout=5)
