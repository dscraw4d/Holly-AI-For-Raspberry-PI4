"""Independent host-only validation. Python is not a kernel dependency."""
import ctypes
import hashlib
import hmac
from pathlib import Path

library = ctypes.CDLL(str(Path('build/crypto_reference.so').resolve()))
hash_fn = library.holly_sha256_hash
hash_fn.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
hash_fn.restype = ctypes.c_int
mac_fn = library.holly_hmac_sha256
mac_fn.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                   ctypes.c_size_t, ctypes.c_void_p]
mac_fn.restype = ctypes.c_int
lengths = [0, 1, 31, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128, 129, 1024, 4097]
for length in lengths:
    data = bytes((i * 29 + length) % 256 for i in range(length))
    digest = ctypes.create_string_buffer(32)
    assert hash_fn(data, len(data), digest) == 0
    assert digest.raw == hashlib.sha256(data).digest(), length
    for key_length in [0, 1, 32, 63, 64, 65, 131]:
        key = bytes((i * 17 + key_length) % 256 for i in range(key_length))
        assert mac_fn(key, len(key), data, len(data), digest) == 0
        assert digest.raw == hmac.digest(key, data, 'sha256'), (length, key_length)
print('Independent SHA-256/HMAC reference: 128 boundary cases passed')
