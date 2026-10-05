#!/usr/bin/env python3
"""Boot the exact Pi 4 kernel binary under QEMU 9+; not a firmware/hardware test."""
import argparse
import json
import os
from pathlib import Path
import selectors
import re
import subprocess
import time

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--qemu', default='qemu-system-aarch64')
parser.add_argument('--require-network',action='store_true')
parser.add_argument('--runs', type=int, default=3)
parser.add_argument('--soak-seconds', type=int, default=0)
args = parser.parse_args()
logs = []
command_cycles = 0
for run in range(args.runs):
    process = subprocess.Popen([args.qemu, '-M', 'raspi4b', '-m', '2G', '-smp', '4',
        '-kernel', str(root / 'build/kernel8.img'), '-display', 'none',
        '-serial', 'stdio', '-monitor', 'none', '-nic', 'none', '-no-reboot'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    sel = selectors.DefaultSelector()
    sel.register(process.stdout, selectors.EVENT_READ)
    transcript = bytearray()
    def until(marker, start=0, timeout=30):
        deadline = time.monotonic() + timeout
        while marker not in transcript[start:]:
            if time.monotonic() >= deadline or process.poll() is not None:
                raise RuntimeError(f'Boot {run+1}: missing {marker!r}: {transcript[-2000:]!r}')
            for key, _ in sel.select(0.1):
                data = os.read(key.fd, 65536)
                if data:
                    transcript.extend(data)
    def command(text, expected):
        global command_cycles
        command_cycles += 1
        start = len(transcript)
        process.stdin.write(text.encode('ascii') + b'\r\n')
        process.stdin.flush()
        until(b'holly> ', start)
        assert expected.encode('ascii') in transcript[start:], transcript[start:]
    try:
        until(b'holly> ')
        if args.require_network:
            assert b'SSH network disabled: this build has no provisioned credentials.' not in transcript, 'Unprovisioned kernel booted'
        command('version', '0.49.29')
        command('what is 10 plus 10?', '20.')
        command('what is ten plus ten?', '20.')
        command('calculate (2 + 3) * 4', '20.')
        command('calculate 7 divided by 2', '3.5.')
        command('calculate 1 / 0', 'divide by zero')
        command('NEWS', 'clock')
        command('discussion status', 'discussion is on')
        command('What do you think about Rimmer?', 'Rimmer')
        command('I think Rimmer is ridiculous', 'What makes you say')
        command('Why?', 'About Arnold Rimmer')
        command('What if Rimmer were captain?', 'Hypothetical')
        command('Why?', 'my reasoning')
        command('source', 'Basis for my interpretation')
        command('chat reset', 'chat is off')
        command('What were we talking about?', "haven't picked a topic")
        command('brain reset', 'retrieval is on')
        command('repeat that', "haven't answered")
        command('script list', 'Episode script bank unavailable')
        command('How did Lister survive the radiation leak?', 'stasis')
        command('Who played him?', 'Craig Charles')
        command('Where was he born?', 'Liverpool')
        command('source', 'reddwarf.co.uk')
        command('What is Lister favourite programming language?', "online reader is unavailable")
        command('brain reset', 'retrieval is on')
        command('storagediag', 'SD init stage:')
        command('displaydiag', 'Inference worker cores: 3')
        command('displaydiag', 'Port 0 framebuffer: framebuffer ready')
        command('displaydiag', 'Port 1 framebuffer: not attempted')
        command('displaydiag', 'Video bring-up: fullscreen RGB565 portrait;')
        command('display frame 4', 'Display command result (0=completed): 0')
        command('displaydiag', 'Mouth frame (1-7): 4')
        command('display frame 7', 'Display command result (0=completed): 0')
        command('displaydiag', 'Mouth frame (1-7): 7')
        command('display face', 'Display command result (0=completed): 0')
        command('display animate off', 'Display command result (0=completed): 0')
        command('displaydiag', 'Text mouth animation enabled: 0')
        command('display animate on', 'Display command result (0=completed): 0')
        command('dwarf status', '42 curated topics')
        command('who is Lister', 'stasis')
        command('who plays him', 'Craig Charles')
        command('dwarf source', 'theend')
        command('dwarf Holly', '6000')
        command('tell me more', 'Hilly')
        command('dwarf off', 'off')
        command('dwarf on', 'on')
        command('personality status', 'banter is on')
        command('hello', 'Ship computer here')
        command('model status', '39,986 trained parameters')
        command('train status', 'RAM ONLY')
        command('train start 8', 'OK output-layer training queued')
        command('train pause', 'OK training controls queued')
        command('model generate Holly is', json.loads((root/'release-evidence/model-v0.34.json').read_text())['samples']['Holly is'])
        command('how are you', 'social life')
        command('personality off', 'banter is off')
        command('chat on', '1266121 parameters')
        command('chat model 1', '127647 parameters')
        command('how are you', 'running and ready to chat')
        command('thanks holly', 'You are welcome')
        command('chat reset', 'chat is on')
        command('chat model 3', '792193 parameters')
        command('i enjoy red dwarf', 'You mentioned red dwarf')
        command('what would you like to know', 'What do you like about red dwarf?')
        command('chat model 4', '1266121 parameters')
        command('chat reset', 'chat is on')
        command('how are you', 'running and ready to chat')
        command('chat off', 'chat is off')
        command('cryptodiag', 'self-test passed')
        command('hash abc', 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
        command('teach What is your name? => Holly AI Learning OS', 'have that in memory')
        command('ask What is your name?', 'Holly AI Learning OS')
        command('repeat that', 'Holly AI Learning OS')
        command('storage', 'Holly Vault')
        command('timediag', 'Timer microseconds high/low')
        command('displaydiag', 'HDMI:')
        command('display white', 'Display command result (0=completed): 0')
        command('displaydiag', 'Port 0 framebuffer: framebuffer ready')
        command('displaydiag', 'Port 1 framebuffer: not attempted')
        command('hash abx\x08c', 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
        if args.soak_seconds:
            import re
            before = int(re.findall(rb'Frames drawn: ([0-9]+)', transcript)[-1])
            deadline = time.monotonic() + args.soak_seconds
            while time.monotonic() < deadline:
                for key, _ in sel.select(0.1):
                    data = os.read(key.fd, 65536)
                    if data:
                        transcript.extend(data)
                if process.poll() is not None:
                    raise RuntimeError('Emulator stopped during idle soak')
            command('displaydiag', 'Port 0 framebuffer: framebuffer ready')
            after = int(re.findall(rb'Frames drawn: ([0-9]+)', transcript)[-1])
            assert after == before and after > 0, 'Static test pattern was redrawn'
        command('My Name is Darren', "Hi Darren. I'm Holly")
        command('hello', 'Hi Darren')
        command('What do you know about season 1?', 'six episodes from 1988')
        command('tell me more', 'first series')
        command('source', 'official Red Dwarf')
        command('who is cat', 'Cat')
        command('What do you know about season 8?', 'Only the Good')
        command('which episodes?', 'Back in the Red, part 3')
        command('tell me more', 'particular episode')
        command('source', 'official Red Dwarf')
        command('networkdiag', 'Network recovery attempts:')
        for _ in range(20):
            command('status', 'Chat active')
        logs.append(f'=== Cold boot {run+1} ===\n' + transcript.decode('ascii', errors='replace'))
    finally:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        sel.close()
(root / 'build/qemu-smoke.log').write_text('\n'.join(logs))
print(f'{args.runs} Pi 4 emulated cold boots and {command_cycles} command cycles passed; idle soak {args.soak_seconds}s')
