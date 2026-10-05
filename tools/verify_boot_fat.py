#!/usr/bin/env python3
"""Independent read-only checks for the boot FAT32 and shipped kernel file."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
IMAGE = ROOT / 'dist/Holly-AI-Learning-OS-v0.49-Pi4-JMC-Conversation.img'
KERNEL = ROOT / 'build/kernel8.img'
SECTOR = 512

def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]

def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]

with IMAGE.open('rb') as disk:
    def read_at(offset, size):
        disk.seek(offset)
        data = disk.read(size)
        assert len(data) == size
        return data

    mbr = read_at(0, SECTOR)
    assert mbr[510:512] == b'\x55\xaa'
    assert mbr[450] == 0x0c and mbr[466] == 0xda
    start, sectors = struct.unpack_from('<II', mbr, 454)
    vault_start, vault_sectors = struct.unpack_from('<II', mbr, 470)
    assert start == 2048 and start + sectors == vault_start == 458752
    assert vault_sectors == 65536 and IMAGE.stat().st_size == 256 * 1024 * 1024

    boot = read_at(start * SECTOR, SECTOR)
    assert boot[510:512] == b'\x55\xaa' and boot[82:90] == b'FAT32   '
    assert u16(boot, 11) == SECTOR and u32(boot, 28) == start
    assert u32(boot, 32) == sectors and boot[16] == 2
    cluster_sectors = boot[13]
    reserved = u16(boot, 14)
    fat_sectors = u32(boot, 36)
    root_cluster = u32(boot, 44)
    assert cluster_sectors and cluster_sectors & (cluster_sectors - 1) == 0
    assert reserved >= 32 and fat_sectors and root_cluster >= 2
    backup = u16(boot, 50)
    assert read_at((start + backup) * SECTOR, SECTOR) == boot
    info = read_at((start + u16(boot, 48)) * SECTOR, SECTOR)
    assert u32(info, 0) == 0x41615252 and u32(info, 484) == 0x61417272
    assert info[510:512] == b'\x55\xaa'

    fat_offset = (start + reserved) * SECTOR
    fat = read_at(fat_offset, fat_sectors * SECTOR)
    assert read_at(fat_offset + fat_sectors * SECTOR, len(fat)) == fat
    data_sector = start + reserved + 2 * fat_sectors
    cluster_bytes = cluster_sectors * SECTOR

    def next_cluster(cluster):
        return u32(fat, cluster * 4) & 0x0fffffff

    def chain(first):
        seen = set()
        while 2 <= first < 0x0ffffff8:
            assert first not in seen and first * 4 + 4 <= len(fat)
            seen.add(first)
            yield read_at((data_sector + (first - 2) * cluster_sectors) * SECTOR,
                          cluster_bytes)
            first = next_cluster(first)
        assert first >= 0x0ffffff8

    entry = None
    for block in chain(root_cluster):
        for offset in range(0, len(block), 32):
            record = block[offset:offset + 32]
            if record[0] == 0:
                break
            if record[0] != 0xe5 and record[11] != 0x0f and record[:11] == b'KERNEL8 IMG':
                entry = record
        if entry:
            break
    assert entry is not None, 'KERNEL8.IMG short entry missing'
    size = u32(entry, 28)
    first = u16(entry, 26) | (u16(entry, 20) << 16)
    assert size == KERNEL.stat().st_size
    contents = b''.join(chain(first))[:size]
    assert contents == KERNEL.read_bytes(), 'FAT cluster chain differs from kernel8.img'

print(f'Independent FAT32 boot check passed: {size} kernel bytes, '
      f'{cluster_sectors} sector clusters, two matching FATs')
