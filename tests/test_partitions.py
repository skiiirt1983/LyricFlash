"""Regression test against the user's actual uploaded full flash image.
Runs without ESP-IDF. By design does not expose saved NVS credentials.
"""
import csv
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
IMAGE = pathlib.Path('/mnt/data/printsphere_full.bin')
EXPECTED = {
    'nvs': (0x9000, 0x100000),
    'otadata': (0x109000, 0x2000),
    'phy_init': (0x10b000, 0x1000),
    'ota_0': (0x110000, 0x500000),
    'ota_1': (0x610000, 0x500000),
    'sounds': (0xb10000, 0x4f0000),
}

def part_csv():
    rows = {}
    with (ROOT / 'partitions.csv').open() as f:
        for parts in csv.reader(line for line in f if not line.lstrip().startswith('#')):
            if not parts or not parts[0].strip():
                continue
            name = parts[0].strip()
            rows[name] = (int(parts[3], 0), int(parts[4], 0))
    return rows

def original():
    if not IMAGE.exists():
        return None
    data = IMAGE.read_bytes()
    entries = {}
    for i in range(32):
        part = data[0x8000+i*32:0x8000+(i+1)*32]
        if part[:2] != b'\xaa\x50': break
        _, typ, subtype, offset, size, label, flags = struct.unpack('<HBBII16sI',part)
        entries[label.split(b'\0')[0].decode()] = (offset, size)
    return entries

if __name__ == '__main__':
    assert part_csv() == EXPECTED, (part_csv(), EXPECTED)
    if original() is not None:
        assert original() == EXPECTED, 'Partition table differs from uploaded image'
        print('PASS: alpha partitions match actual PrintSphere full image')
    else:
        print('PASS: expected partitions (uploaded original not mounted here)')
    assert max(o+s for o,s in EXPECTED.values()) == 0x1000000
    print('PASS: entire 16MB flash accounted for')
