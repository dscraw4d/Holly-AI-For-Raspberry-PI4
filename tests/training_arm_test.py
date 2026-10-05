import argparse
from pathlib import Path
import re
import selectors
import os
import subprocess
import time
p=argparse.ArgumentParser();p.add_argument('--qemu',required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1]
expected=re.search(r'CANDIDATE25 [0-9a-f]{8}',(root/'build/selftrain-v0.35.log').read_text()).group()
proc=subprocess.Popen([a.qemu,'-M','raspi4b','-m','2G','-smp','4','-kernel',str(root/'build/training-arm-test.img'),'-display','none','-serial','stdio','-monitor','none','-nic','none'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
data=bytearray();sel=selectors.DefaultSelector();sel.register(proc.stdout,selectors.EVENT_READ)
try:
    deadline=time.monotonic()+30
    while b'\n' not in data:
        if time.monotonic()>deadline:raise TimeoutError(data)
        if sel.select(.1):data.extend(os.read(proc.stdout.fileno(),4096))
    assert expected.encode() in data,(expected,data)
    print('AArch64 25 integer training updates match host candidate CRC:',expected)
finally:
    sel.close();proc.terminate();proc.wait(timeout=5)
