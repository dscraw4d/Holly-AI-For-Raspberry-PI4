"""Real OpenSSH interoperability through a test-only loopback byte adapter."""
import base64
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import selectors
import time
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from provision_ssh import provision
with tempfile.TemporaryDirectory(prefix='holly-ssh-') as tmp:
    directory=Path(tmp);password='A unique test password '+os.urandom(12).hex()
    creds,pub,fingerprint=provision(directory,'holly',password)
    askpass=directory/'askpass'
    askpass.write_text('#!/usr/bin/python3\nimport os\nprint(os.environ["HOLLY_TEST_PASSWORD"])\n')
    askpass.chmod(0o700)
    env=dict(os.environ,SSH_ASKPASS=str(askpass),SSH_ASKPASS_REQUIRE='force',DISPLAY='holly-test',HOLLY_TEST_PASSWORD=password)
    for fragment in [1,17,4096]:
        server=subprocess.Popen([str(ROOT/'build/ssh_host_adapter'),str(creds),str(fragment)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            line=server.stdout.readline();assert line.startswith('PORT '),line
            port=int(line.split()[1]);known=directory/'known_hosts'
            known.write_text(f'[127.0.0.1]:{port} ssh-rsa '+base64.b64encode(pub).decode()+'\n')
            cmd=['ssh','-F','/dev/null','-T','-p',str(port),'-o','ConnectTimeout=5','-o','StrictHostKeyChecking=yes',
                 '-o',f'UserKnownHostsFile={known}','-o','PreferredAuthentications=password','-o','PubkeyAuthentication=no','holly@127.0.0.1']
            for command,expected in [('version','0.49'),('storagediag','diagnostics unavailable'),('displaydiag','diagnostics unavailable'),('display bars','diagnostics unavailable'),('hello','Ship computer here'),('dwarf status','42 curated topics'),('dwarf Lister','stasis'),('who is Kryten','mechanoid'),('teach sky colour => blue','have that in memory'),('ask sky colour','Holly: blue'),('help','teach question'),('train status','Training phase'),('train export 0','EXAMPLE 0'),('model status','39,986 trained parameters'),('model generate Holly is','experimental completion')]:
                result=subprocess.run(cmd+[command],env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,timeout=20)
                assert result.returncode==0,(fragment,command,result.returncode,result.stdout,result.stderr)
                assert expected in result.stdout,result.stdout
            result=subprocess.run(cmd,input='hello\nteach favourite ship => Red Dwarf\nask favourite ship\nrepeat that\nexit\n',env=env,capture_output=True,text=True,timeout=20)
            assert result.returncode==0,(result.stdout,result.stderr)
            assert result.stdout.count('Holly: Red Dwarf')==2,result.stdout
            dialogue=subprocess.run(cmd,input='personality off\nchat model 1\nchat on\nhow are you\nthanks holly\nchat reset\ni enjoy red dwarf\nwhat would you like to know\nchat off\nexit\n',env=env,capture_output=True,text=True,timeout=30)
            assert dialogue.returncode==0,(dialogue.stdout,dialogue.stderr)
            assert 'running and ready to chat' in dialogue.stdout,dialogue.stdout
            assert 'You are welcome' in dialogue.stdout,dialogue.stdout
            assert 'What do you like about red dwarf?' in dialogue.stdout,dialogue.stdout
            lore=subprocess.run(cmd,input='dwarf on\nwho is Lister\nwho plays him\ndwarf source\ndwarf Holly\ntell me more\npersonality on\nchat off\ntell me a joke\nexit\n',env=env,capture_output=True,text=True,timeout=30)
            assert lore.returncode==0,(lore.stdout,lore.stderr)
            assert 'Craig Charles' in lore.stdout and 'theend' in lore.stdout and 'Hilly' in lore.stdout,lore.stdout
            pty_cmd=[('-tt' if part=='-T' else part) for part in cmd]
            result=subprocess.run(pty_cmd,input='hellx\x7fo\nexit\n',env=env,capture_output=True,text=True,timeout=20)
            assert result.returncode==0 and 'Ship computer here' in result.stdout,(result.stdout,result.stderr)
            # Force repeated renewal while the same authenticated chat stays open.
            with (directory/'rekey.log').open('w+b') as debug:
                renewal=subprocess.Popen(cmd[:-1]+['-vv','-o','RekeyLimit=1K',cmd[-1]],env=env,
                    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=debug)
                selector=selectors.DefaultSelector();selector.register(renewal.stdout,selectors.EVENT_READ)
                transcript=bytearray()
                def prompt(start):
                    deadline=time.monotonic()+20
                    while b'holly> ' not in transcript[start:]:
                        assert time.monotonic()<deadline and renewal.poll() is None,bytes(transcript[-1000:])
                        if selector.select(.1):
                            part=os.read(renewal.stdout.fileno(),65536)
                            assert part
                            transcript.extend(part)
                try:
                    prompt(0)
                    for command in [b'personality off\n',b'teach current course => Jupiter\n',b'ask current course\n']:
                        start=len(transcript);renewal.stdin.write(command);renewal.stdin.flush();prompt(start)
                    for turn in range(60):
                        start=len(transcript);renewal.stdin.write(b'hello\n');renewal.stdin.flush();prompt(start)
                        assert b'Ship computer here' in transcript[start:]
                    start=len(transcript);renewal.stdin.write(b'repeat that\n');renewal.stdin.flush();prompt(start)
                    assert b'Holly: Jupiter' in transcript[start:]
                    renewal.stdin.write(b'exit\n');renewal.stdin.flush()
                    remaining,_=renewal.communicate(timeout=20)
                    assert renewal.returncode==0,remaining
                    debug.seek(0);trace=debug.read()
                    cycles=trace.count(b'SSH2_MSG_NEWKEYS received')
                    assert cycles>=3,trace[-3000:]
                    print(f'OpenSSH: fragment {fragment}: 60 uninterrupted turns across {cycles} key exchanges passed')
                finally:
                    selector.close()
                    if renewal.poll() is None:renewal.kill();renewal.wait()
            wrong=dict(env,HOLLY_TEST_PASSWORD='This is the incorrect password')
            result=subprocess.run(cmd+['hello'],env=wrong,stdin=subprocess.DEVNULL,capture_output=True,text=True,timeout=20)
            assert result.returncode!=0 and 'Permission denied' in result.stderr,(result.stdout,result.stderr)
            print(f'OpenSSH: fragment {fragment}: 11 exec commands, interactive chat, PTY/backspace and incorrect-password rejection passed')
        finally:
            server.terminate();server.wait(timeout=5)
    print('OpenSSH SSH-2.0 interoperability passed; pinned host identity '+fingerprint)
