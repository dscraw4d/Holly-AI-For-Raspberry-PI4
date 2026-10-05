#!/usr/bin/env python3
"""Package the verified SSH image/source and its unique private identity."""
import argparse
import hashlib
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--credentials',type=Path,required=True)
parser.add_argument('--output',type=Path,default=ROOT.parent/'Holly-AI-Learning-OS-v0.49-Pi4-JMC-Conversation.zip')
args=parser.parse_args()
image=ROOT/'dist/Holly-AI-Learning-OS-v0.49-Pi4-JMC-Conversation.img'
credentials=args.credentials.resolve()
if args.output.exists() and not args.output.is_file():raise SystemExit('Output must be a regular file')
assert (credentials/'ssh_credentials_generated.h').read_bytes()==(ROOT/'build/ssh_credentials_generated.h').read_bytes()
assert (ROOT/'build/kernel8.img').read_bytes()==(ROOT/'bootfiles/kernel8.img').read_bytes()
docs=['README.md','VALIDATION.md','PI4-BENCH-TEST.md','SSH-IMPLEMENTATION.md','HTTPS-IMPLEMENTATION.md','LANGUAGE-MODEL.md','SELF-TRAINING.md','VISION.md','UPDATE-INSTRUCTIONS.txt','FIRST-BOOT-v0.49.txt','JMC-BOOT-v0.45.md','SCRIPTS-v0.44.md','MODEL-v0.43.md','ACCESS-AND-DOCUMENTS.md','VALIDATION-v0.49.txt','CONVERSATION-v0.41.md','PERSONALITY-v0.42.md','PI-ONLY-v0.46.md','DISCUSSION-v0.47.md','MODEL-v0.48.md','RELEASE-v0.49.md','UPDATE-v0.49.txt']
with zipfile.ZipFile(args.output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    z.write(image,image.name);z.write(ROOT/'build/kernel8.img','kernel8.img')
    z.write(ROOT/'bootfiles/config.txt','config.txt')
    z.write(ROOT/'assets/JMC-Splash-Preview.png','JMC-Splash-Preview.png')
    z.write(ROOT/'assets/JMC-Dashboard-v0.49.png','JMC-Dashboard-v0.49.png')
    z.write(ROOT/'tools/wiki_reader.py','wiki_reader.py')
    z.write(ROOT/'tools/holly_upload.py','holly_upload.py')
    z.write(ROOT/'tools/holly_scripts.py','holly_scripts.py')
    z.write(ROOT/'build/kernel.elf','debug/kernel.elf');z.write(ROOT/'dist/SHA256SUMS.txt','SHA256SUMS.txt')
    for name in docs:z.write(ROOT/name,name)
    z.write(credentials/'CONNECTION.txt','CONNECTION.txt')
    for name in ['ssh-credentials.bin','ssh_credentials_generated.h','ssh-host-key.pub','ssh-host-fingerprint.txt']:
        z.write(credentials/name,'credentials/'+name)
    for folder in ['bootfiles','UPSTREAM-LICENSES','release-evidence']:
        for path in sorted((ROOT/folder).rglob('*')):
            if path.is_file():z.write(path,str(path.relative_to(ROOT)))
    excluded={'build','bootfiles','dist','private-release','release-evidence','UPSTREAM-LICENSES','.git'}
    for path in sorted(ROOT.rglob('*')):
        if not path.is_file():continue
        rel=path.relative_to(ROOT)
        if str(rel).startswith('vendor/bearssl/build/'):continue
        if rel.parts[0] in excluded or '__pycache__' in rel.parts or path.suffix in {'.pyc','.zip'}:continue
        z.write(path,'source/Holly-AI-Learning-OS/'+str(rel))
with zipfile.ZipFile(args.output) as z:
    assert z.testzip() is None
    assert z.getinfo(image.name).file_size==256*1024*1024
    sums=dict((line.split()[1],line.split()[0]) for line in z.read('SHA256SUMS.txt').decode().splitlines())
    for name in ['kernel8.img',image.name]:assert hashlib.sha256(z.read(name)).hexdigest()==sums[name]
    assert not any('arm-test/' in n or n.endswith('ssh-arm-test.img') for n in z.namelist())
print(f'Verified {args.output.name}: {args.output.stat().st_size} bytes, image hashes and ZIP CRC passed')
