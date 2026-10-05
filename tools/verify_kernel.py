#!/usr/bin/env python3
"""Validate the compiled AArch64 ELF and its raw Pi kernel image."""
from pathlib import Path
import argparse
import shutil
import struct
import subprocess
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--require-network',action='store_true')
parser.add_argument('--credentials',type=Path)
args=parser.parse_args()
root = Path(__file__).resolve().parent.parent
elf = (root / 'build/kernel.elf').read_bytes()
image = (root / 'build/kernel8.img').read_bytes()
h = struct.unpack_from('<16sHHIQQQIHHHHHH', elf)
assert h[0][:6] == b'\x7fELF\x02\x01'
assert h[1] == 2 and h[2] == 183 and h[4] == 0x80000
assert h[9] == 56
loads = []
for i in range(h[10]):
    p = struct.unpack_from('<IIQQQQQQ', elf, h[5] + i * h[9])
    if p[0] != 1:
        continue
    kind, flags, offset, virtual, physical, size, memory, align = p
    assert virtual == physical and physical >= 0x80000
    assert physical + memory < 0x1000000 and memory >= size
    assert flags & 3 != 3, 'Writable executable ELF segment'
    assert offset + size <= len(elf)
    loads.append(p)
assert loads and min(p[4] for p in loads) == h[4]
end = max(p[4] + p[5] for p in loads if p[5])
expected = bytearray(end - h[4])
for p in loads:
    expected[p[4] - h[4]:p[4] - h[4] + p[5]] = elf[p[2]:p[2] + p[5]]
assert image == expected, 'Raw image differs from ELF loadable bytes'
nm = shutil.which('aarch64-linux-gnu-nm') or shutil.which('nm')
assert nm, 'Install binutils to inspect undefined ELF symbols'
undefined = subprocess.check_output([nm, '-u', str(root / 'build/kernel.elf')], text=True)
assert not undefined.strip(), undefined
print(f'AArch64 entry 0x80000; {len(image)} raw bytes; no unresolved symbols; ELF/image match')

if args.require_network:
    assert args.credentials, 'Release check requires the pinned credential file'
    symbols=subprocess.check_output([nm,'-S','--defined-only',str(root/'build/kernel.elf')],text=True)
    entries={parts[-1]:int(parts[1],16) for line in symbols.splitlines() if len(parts:=line.split())==4}
    assert entries.get('holly_pi_network_start',0)>256, 'Pi networking is not provisioned: stub initialization in release kernel'
    assert entries.get('rx_buffers',0)==256*2048 and entries.get('tx_buffers',0)==256*2048, 'Missing Pi Ethernet DMA buffers'
    credentials=args.credentials.read_bytes()
    assert credentials[:8]==b'HLYSSH23' and len(credentials)==617, 'Invalid pinned credential file'
    assert credentials[8:] in image, 'Release kernel does not embed the pinned SSH credentials'
    assert 'telnet_enabled' in entries and 'http_enabled' in entries, 'Missing Pi LAN services'
    print('Release gate: real Pi network implementation, Ethernet buffers, LAN services and exact pinned credentials embedded')
