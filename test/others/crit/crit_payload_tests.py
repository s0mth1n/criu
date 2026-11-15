#!/usr/bin/env python3
import io
import json
import os
import struct
import subprocess
import sys
import unittest

from pycriu import images
from pycriu.images import magic, pb


def make_image(name, entries):
    image = io.BytesIO()
    image.write(struct.pack('ii', magic.by_name['IMG_COMMON'],
                            magic.by_name[name]))
    for entry, payload in entries:
        data = entry.SerializeToString()
        image.write(struct.pack('i', len(data)))
        image.write(data)
        image.write(payload)
    return image.getvalue()


class PayloadTests(unittest.TestCase):
    def check_roundtrip(self, raw):
        for pretty in (False, True):
            with self.subTest(pretty=pretty):
                decoded = images.loads(raw, pretty=pretty)
                decoded = json.loads(json.dumps(decoded))
                self.assertEqual(images.dumps(decoded), raw)

    def test_bpfmap(self):
        for count in (0, 1, 2):
            with self.subTest(count=count):
                entries = []
                for map_id in (1, 2):
                    payload = bytes(range(256)) * count
                    entry = pb.bpfmap_data_entry(
                        map_id=map_id, keys_bytes=4 * count,
                        values_bytes=252 * count, count=count)
                    entries.append((entry, payload))
                self.check_roundtrip(make_image('BPFMAP_DATA', entries))

    def test_legacy_shm(self):
        for size in (0, 1, 4, 5, 256):
            with self.subTest(size=size):
                entry = pb.ipc_shm_entry(size=size)
                for field, value in dict(key=1, uid=0, gid=0, cuid=0,
                                         cgid=0, mode=0o600, id=1).items():
                    setattr(entry.desc, field, value)
                payload = bytes(range(size))
                payload += b'\0' * (-size % 4)
                self.check_roundtrip(make_image('IPCNS_SHM', [(entry, payload)]))

    def test_cli(self):
        entry = pb.bpfmap_data_entry(map_id=1, keys_bytes=4,
                                    values_bytes=4, count=1)
        raw = make_image('BPFMAP_DATA', [(entry, b'\x00\xffkeyval')])
        root = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
        env = dict(os.environ, PYTHONPATH=os.pathsep.join(
            (os.path.join(root, 'lib'), os.path.join(root, 'crit'))))
        decoded = subprocess.run([sys.executable, '-m', 'crit', 'decode'],
                                 input=raw, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, env=env)
        self.assertEqual(decoded.returncode, 0, decoded.stderr)
        encoded = subprocess.run([sys.executable, '-m', 'crit', 'encode'],
                                 input=decoded.stdout, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, env=env)
        self.assertEqual(encoded.returncode, 0, encoded.stderr)
        self.assertEqual(encoded.stdout, raw)


if __name__ == '__main__':
    unittest.main()
