#!/usr/bin/env python3
"""Build and verify a regular-file Pi SD image; never opens a physical disk."""
from pathlib import Path
import hashlib
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parent.parent
BOOT = ROOT / 'bootfiles'
BUILD = ROOT / 'build'
DIST = ROOT / 'dist'
SIZE = 256 * 1024 * 1024
START = 2048
DATA_START = 458752             # 224 MiB, aligned and outside the boot FAT
DATA_SECTORS = 65536            # 32 MiB Holly Vault journal
BOOT_SECTORS = DATA_START - START
IMAGE = DIST / 'Holly-AI-Learning-OS-v0.49.29-Lite-Pi4.img'
PARTITION = BUILD / 'boot-partition.fat'
REQUIRED = ['config.txt', 'kernel8.img', 'start4.elf', 'fixup4.dat',
            'bcm2711-rpi-4-b.dtb', 'overlays/disable-bt.dtbo', 'LICENCE.broadcom',
            'FIRMWARE-REVISION.txt']

def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout

system_fat_tools = all(shutil.which(tool) for tool in
                       ['mkfs.fat', 'mcopy', 'mmd', 'fsck.fat'])
if not system_fat_tools:
    try:
        from pyfatfs.PyFat import PyFat
        from pyfatfs.PyFatFS import PyFatFS
    except ImportError as exc:
        raise SystemExit('Install dosfstools/mtools or pyfatfs with setuptools<81') from exc
for name in REQUIRED:
    if not (BOOT / name).is_file() or not (BOOT / name).stat().st_size:
        raise SystemExit(f'Missing boot file: {name}')
if (BOOT / 'kernel8.img').read_bytes() != (BUILD / 'kernel8.img').read_bytes():
    raise SystemExit('Staged kernel differs from the compiled kernel')
nm = shutil.which('aarch64-linux-gnu-nm') or shutil.which('nm')
if not nm:
    raise SystemExit('Install binutils to verify provisioned SSH symbols')
symbols = run(nm, str(BUILD / 'kernel.elf'))
if ' holly_credentials\n' not in symbols:
    raise SystemExit('SSH candidate images require a provisioned kernel; use build-ssh-image.sh')
BUILD.mkdir(exist_ok=True)
DIST.mkdir(exist_ok=True)
for path in [IMAGE, PARTITION]:
    if path.exists() and not path.is_file():
        raise SystemExit(f'Refusing non-regular output: {path}')
with PARTITION.open('wb') as f:
    f.truncate(BOOT_SECTORS * 512)
if system_fat_tools:
    run('mkfs.fat', '--invariant', '-F', '32', '-h', str(START), '-n', 'HOLLYBOOT', str(PARTITION))
    run('mmd', '-i', str(PARTITION), '::overlays')
    for name in REQUIRED:
        run('mcopy', '-o', '-i', str(PARTITION), str(BOOT / name), '::' + name)
    fsck = run('fsck.fat', '-n', str(PARTITION))
else:
    formatter = PyFat()
    formatter.mkfs(str(PARTITION), PyFat.FAT_TYPE_FAT32, size=BOOT_SECTORS * 512,
                   label='HOLLYBOOT', volume_id=0x484c5932)
    formatter.close()
    with PyFatFS(str(PARTITION)) as fat:
        fat.makedir('/overlays')
        for name in REQUIRED:
            with (BOOT / name).open('rb') as source, fat.openbin('/' + name, 'w') as target:
                shutil.copyfileobj(source, target, length=1024 * 1024)
    # The Pi reads this filesystem as a partition beginning at LBA START.
    with PARTITION.open('r+b') as fat_file:
        boot = fat_file.read(512)
        backup = struct.unpack_from('<H', boot, 50)[0]
        assert 0 < backup < 32
        for sector in (0, backup):
            fat_file.seek(sector * 512 + 28)
            fat_file.write(struct.pack('<I', START))
    fsck = 'PyFatFS FAT32 creation and file readback; fsck.fat unavailable.\n'
mbr = bytearray(512)
mbr[440:444] = b'HLY2'
mbr[446:462] = struct.pack('<B3sB3sII', 0x80, bytes([32,33,0]), 0x0c,
                           bytes([254,255,255]), START, BOOT_SECTORS)
mbr[462:478] = struct.pack('<B3sB3sII', 0x00, bytes([254,255,255]), 0xda,
                           bytes([254,255,255]), DATA_START, DATA_SECTORS)
mbr[510:512] = b'\x55\xaa'
with IMAGE.open('wb') as out:
    out.truncate(SIZE)
    out.write(mbr)
    out.seek(START * 512)
    with PARTITION.open('rb') as source:
        shutil.copyfileobj(source, out, length=1024 * 1024)
assert IMAGE.stat().st_size == SIZE
with IMAGE.open('rb') as f:
    header = f.read(512)
    assert header[510:512] == b'\x55\xaa'
    assert struct.unpack_from('<II', header, 454) == (START, BOOT_SECTORS)
    assert struct.unpack_from('<II', header, 470) == (DATA_START, DATA_SECTORS)
    f.seek(START * 512)
    bpb = f.read(512)
    assert bpb[82:90] == b'FAT32   ' and bpb[510:512] == b'\x55\xaa'
    assert struct.unpack_from('<I', bpb, 28)[0] == START
    f.seek(DATA_START * 512)
    assert f.read(512) == bytes(512)
# Read each file back out of the finished disk image, not the staging folder.
VERIFY = BUILD / 'image-readback'
VERIFY.mkdir(exist_ok=True)
if system_fat_tools:
    for name in REQUIRED:
        destination = VERIFY / Path(name).name
        run('mcopy', '-o', '-i', str(IMAGE) + '@@' + str(START * 512), '::' + name, str(destination))
        assert destination.read_bytes() == (BOOT / name).read_bytes(), name
else:
    with PyFatFS(str(IMAGE), offset=START * 512, read_only=True) as fat:
        for name in REQUIRED:
            contents = fat.readbytes('/' + name)
            (VERIFY / Path(name).name).write_bytes(contents)
            assert contents == (BOOT / name).read_bytes(), name
hashes = []
for path in [IMAGE, BOOT / 'kernel8.img']:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda: f.read(1024 * 1024), b''):
            h.update(data)
    hashes.append(f'{h.hexdigest()}  {path.name}')
(DIST / 'SHA256SUMS.txt').write_text('\n'.join(hashes) + '\n')
(BUILD / 'image-verification.txt').write_text(fsck + '\nMBR, FAT32 and all boot files verified by readback.\n')
print(f'Verified {IMAGE} ({SIZE} bytes)')
