"""Numerical learning checks and bit-exact Python/C deployment checks."""
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'model'))
import holly_lm as lm
rng=np.random.default_rng(101)
p=lm.initialize(101)
x=rng.integers(lm.VOCAB,size=(3,lm.CTX)); y=np.array([1,30,60])
loss,g=lm.loss_grad(p,x,y)
# Central finite differences check each parameter family, including embeddings.
for name,shape in lm.SHAPES.items():
    for _ in range(5):
        ix=tuple(int(rng.integers(s)) for s in shape)
        if name=='embedding': ix=(int(x.ravel()[rng.integers(x.size)]),ix[1])
        original=p[name][ix]; eps=1e-5
        p[name][ix]=original+eps; plus=lm.loss_grad(p,x,y,False)
        p[name][ix]=original-eps; minus=lm.loss_grad(p,x,y,False)
        p[name][ix]=original
        assert abs((plus-minus)/(2*eps)-g[name][ix])<1e-7,(name,ix)
for name in p: p[name]-=.01*g[name]
assert lm.loss_grad(p,x,y,False)<loss
trained=lm.load(ROOT/'model/seed1.npz'); q=lm.quantize(trained)
with tempfile.TemporaryDirectory() as d:
    generated=Path(d)/'weights.h';lm.export(trained,generated)
    assert generated.read_bytes()==(ROOT/'src/language_model_weights.h').read_bytes()
    libpath=Path(d)/'lm.so'
    subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC','-Isrc',
                    'src/language_model.c','-o',str(libpath)],cwd=ROOT,check=True)
    lib=ctypes.CDLL(str(libpath))
    U8=ctypes.c_uint8*lm.CTX; I32=ctypes.c_int32*lm.VOCAB
    lib.holly_lm_logits.argtypes=[ctypes.POINTER(ctypes.c_uint8),ctypes.POINTER(ctypes.c_int32)]
    lib.holly_lm_generate.argtypes=[ctypes.c_char_p,ctypes.c_void_p,ctypes.c_uint]
    for _ in range(100):
        context=rng.integers(lm.VOCAB,size=lm.CTX); out=I32()
        lib.holly_lm_logits(U8(*map(int,context)),out)
        assert np.array_equal(list(out),lm.fixed_logits(q,context))
    samples={}
    for prompt in ['', 'Holly is', 'A computer', 'User: hello\nHolly:', 'x'*256]:
        out=ctypes.create_string_buffer(97)
        n=lib.holly_lm_generate(prompt.encode(),out,len(out))
        assert 0<=n<=96 and out.value.decode()==lm.generate(q,prompt)
        samples[prompt[:40]]=out.value.decode()
    for invalid in [b'x'*257,b'\x1b',b'\xff',None]:
        out=ctypes.create_string_buffer(b'old value',97)
        assert lib.holly_lm_generate(invalid,out,len(out))==-1 and out.value==b''
    for capacity in [1,2,8,97]:
        out=ctypes.create_string_buffer(b'Z'*128,129)
        n=lib.holly_lm_generate(b'Holly',out,capacity)
        assert n<capacity and out.raw[n]==0 and out.raw[capacity:128]==b'Z'*(128-capacity)
    assert lib.holly_lm_generate(b'Holly',None,0)==-1
# Test corpus is disjoint by record, and was not used for checkpoint selection.
train,_,_=lm.examples(ROOT/'model/data/train.txt')
valid,vx,vy=lm.examples(ROOT/'model/data/valid.txt')
test,tx,ty=lm.examples(ROOT/'model/data/test.txt')
assert not set(train)&set(valid) and not set(test)&(set(train)|set(valid))
initial=lm.evaluate(lm.initialize(),tx,ty); final=lm.evaluate(trained,tx,ty)
assert final<initial
quantized=lm.evaluate({k:v.astype(float)/lm.SCALE for k,v in q.items()},tx,ty)
assert abs(quantized-final)<.01
report={'gradient_checks':25,'exact_integer_contexts':100,'initial_test_loss':initial,
        'trained_test_loss':final,'quantized_test_loss':quantized,
        'scope':'Six small authored test records. No broad language capability or factual accuracy claim.',
        'samples':samples}
(ROOT/'release-evidence/model-v0.34.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
print('Seed-1 gradient, descent, held-out prediction, export, integer parity and bounds checks passed')
