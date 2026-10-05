#!/usr/bin/env python3
"""Package a kernel-only update without changing an SD card or disk image."""
import argparse
import hashlib
from pathlib import Path
import zipfile
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--version', default='0.39')
p.add_argument('--output', type=Path)
a = p.parse_args()
if not all(c.isdigit() or c == '.' for c in a.version):
    p.error('Version must contain digits and periods only')
out = a.output or ROOT.parent / f'Holly-AI-Learning-OS-v{a.version}-Red-Dwarf-Update.zip'
kernel = ROOT / 'build/kernel8.img'
install = ROOT / f'UPDATE-v{a.version}.txt'
validation = ROOT / f'VALIDATION-v{a.version}.txt'
for file in (kernel, install, validation):
    if not file.is_file():
        p.error(f'Missing {file.name}')
excluded = {'build', '__pycache__', '.git', '.venv'}
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    z.write(kernel, 'kernel8.img')
    z.write(install, 'READ-ME-FIRST.txt')
    z.write(validation, 'VALIDATION.txt')
    z.writestr('SHA256SUMS.txt', hashlib.sha256(kernel.read_bytes()).hexdigest() + '  kernel8.img\n')
    source = 'source/Holly-AI-Learning-OS/'
    for name in ['src', 'tests', 'tools', 'model', 'vendor']:
        for file in sorted((ROOT / name).rglob('*')):
            rel = file.relative_to(ROOT)
            if not file.is_file() or any(part in excluded for part in rel.parts):
                continue
            if file.suffix in {'.o', '.a', '.so', '.pyc', '.zip'} or file.name == 'dialogue1.npz':
                continue
            z.write(file, source + str(rel))
    for file in sorted(ROOT.iterdir()):
        if file.is_file() and (file.suffix in {'.md', '.txt', '.sh'} or file.name in {'Makefile', 'config.txt'}):
            z.write(file, source + file.name)
    for file in sorted((ROOT / 'release-evidence').glob(f'*v{a.version}*')):
        if file.is_file():
            z.write(file, 'evidence/' + file.name)
    # QEMU smoke uses this older Seed-1 reference, whose weights are retained.
    reference = ROOT / 'release-evidence/model-v0.34.json'
    z.write(reference, source + 'release-evidence/' + reference.name)
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None
    assert z.read('kernel8.img') == kernel.read_bytes()
    assert not any('private-release/' in name or 'ssh_credentials_generated.h' in name for name in z.namelist())
print(f'Verified {out.name}: {out.stat().st_size} bytes')
