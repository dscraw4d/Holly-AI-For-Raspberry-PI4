"""Pinned OpenSSH check of the release identity through the host byte adapter.
This exercises SSH authentication/chat, not Pi Ethernet or firmware.
"""
import argparse, os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--credentials',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='holly-release-login-') as tmp:
    d=Path(tmp);askpass=d/'askpass'
    askpass.write_text('#!/usr/bin/python3\nimport os\nprint(os.environ["HOLLY_TEST_PASSWORD"])\n');askpass.chmod(0o700)
    env=dict(os.environ,SSH_ASKPASS=str(askpass),SSH_ASKPASS_REQUIRE='force',DISPLAY='holly-test',HOLLY_TEST_PASSWORD='smeghead')
    server=subprocess.Popen([str(root/'build/ssh_host_adapter'),str(a.credentials/'ssh-credentials.bin'),'4096'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        line=server.stdout.readline();assert line.startswith('PORT '),line;port=int(line.split()[1])
        pub=(a.credentials/'ssh-host-key.pub').read_text().split()[1]
        known=d/'known_hosts';known.write_text(f'[127.0.0.1]:{port} ssh-rsa {pub}\n')
        cmd=['ssh','-F','/dev/null','-T','-p',str(port),'-o','ConnectTimeout=5','-o','StrictHostKeyChecking=yes','-o',f'UserKnownHostsFile={known}','-o','PreferredAuthentications=password','-o','PubkeyAuthentication=no','-o','NumberOfPasswordPrompts=1','Rimmer@127.0.0.1']
        q='version\nwhat is 10 plus 10?\nwhat is ten plus ten?\ncalculate 7 divided by 2\ncalculate 1 / 0\nWhat do you think about Rimmer?\nWhy?\nsource\nWho plays him?\nchat reset\nrepeat that\nexit\n'
        result=subprocess.run(cmd,env=env,input=q,capture_output=True,text=True,timeout=30)
        assert result.returncode==0,(result.stdout,result.stderr)
        for expected in ['0.49.29','20.','3.5.','divide by zero','interpretation','my reasoning','Basis for my interpretation','Chris Barrie',"haven't answered"]:assert expected in result.stdout,(expected,result.stdout)
        bad=subprocess.run(cmd+['version'],env=dict(env,HOLLY_TEST_PASSWORD='incorrect-test-password'),stdin=subprocess.DEVNULL,capture_output=True,text=True,timeout=15)
        assert bad.returncode!=0 and 'Permission denied' in bad.stderr,(bad.stdout,bad.stderr)
        (root/'release-evidence/v0.49.29-release-login.txt').write_text(result.stdout+'\nWrong password rejected; pinned v0.46 host key verified.\nHost byte adapter, not Pi Ethernet.\n')
        print('Release identity: pinned host key, Rimmer/smeghead login, discussion/reason/source/actor/reset conversation and wrong-password rejection passed')
    finally:
        server.terminate();server.wait(timeout=3)
