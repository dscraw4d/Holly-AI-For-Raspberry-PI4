"""Independent differential tests; reference libraries are host tools only."""
import ctypes as C
import random
from pathlib import Path
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives import hashes

lib = C.CDLL(str(Path(__file__).resolve().parents[1] / 'build/ssh_crypto_reference.so'))
class CTR(C.Structure):
    _fields_ = [('keys',C.c_ubyte*176),('counter',C.c_ubyte*16),('stream',C.c_ubyte*16),('used',C.c_uint),('exhausted',C.c_uint)]
def array(b): return (C.c_ubyte*len(b)).from_buffer_copy(b)
rng = random.Random(0x484f4c4c59)
for n in [0,1,15,16,17,31,32,100,4096]:
    key=rng.randbytes(16);iv=rng.randbytes(16);data=rng.randbytes(n)
    expected=Cipher(algorithms.AES(key),modes.CTR(iv)).encryptor().update(data)
    state=CTR();lib.holly_aes_ctr_init(C.byref(state),array(key),array(iv))
    output=array(data)
    for offset in range(0,n,7):
        assert lib.holly_aes_ctr_xor(C.byref(state),C.byref(output,offset),min(7,n-offset))==0
    assert bytes(output)==expected
state=CTR();lib.holly_aes_ctr_init(C.byref(state),array(bytes(16)),array(b'\xff'*16))
assert lib.holly_aes_ctr_xor(C.byref(state),array(bytes(16)),16)==0
assert lib.holly_aes_ctr_xor(C.byref(state),array(bytes(1)),1)==-1
prime=int.from_bytes(bytes((C.c_ubyte*256).in_dll(lib,'holly_dh14_prime')),'big')
def power(base,exponent,modulus,size):
    out=(C.c_ubyte*256)()
    result=lib.holly_modexp2048(array(base.to_bytes(256,'big')),array(exponent.to_bytes(size,'big')),size,
                             array(modulus.to_bytes(256,'big')),out)
    assert result==0
    assert int.from_bytes(bytes(out),'big')==pow(base,exponent,modulus)
for i in range(16):
    modulus=prime if i<8 else rng.getrandbits(2048)|(1<<2047)|1
    power(rng.randrange(modulus),rng.getrandbits(256),modulus,32)
for base in [0,1,2,prime-1]:
    for e in [0,1,2,65537]:power(base,e,prime,4)
for i in range(12):
    modulus=rng.getrandbits(2048)|(1<<2047)|1
    power(rng.randrange(modulus),rng.getrandbits(2048),modulus,256)
for base in [prime-2,(1<<64)-1,1<<64,(1<<128)-1]:
    for exponent in [0,15,16,255,256]:power(base,exponent,prime,2)
key=rsa.generate_private_key(public_exponent=65537,key_size=2048).private_numbers()
power(rng.randrange(key.public_numbers.n),key.d,key.public_numbers.n,256)
print('SSH crypto: AES/CTR streaming, wrap rejection, 65 modular exponentiation reference cases passed')
