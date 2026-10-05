"""Check original gradients, fixed-point parity, bounded output and context replies."""
import ctypes,importlib.util,json,subprocess
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('dialogue',root/'model/dialogue/dialogue.py');d=importlib.util.module_from_spec(spec);spec.loader.exec_module(d)
rows=d.read_rows(root/'model/dialogue/corpus.jsonl');vocab=d.vocabulary(rows)
p=d.init(vocab);p={k:a.astype(np.float64) for k,a in p.items()};data=d.dataset(rows,vocab,'train');batch=tuple(a[:3] for a in data)
loss,g=d.loss_grad(p,*batch);rng=np.random.default_rng(17)
for k in d.KEYS:
 for _ in range(4):
  index=tuple(int(rng.integers(n)) for n in p[k].shape);old=p[k][index];eps=1e-5
  p[k][index]=old+eps;hi=d.loss_grad(p,*batch,grad=False)
  p[k][index]=old-eps;lo=d.loss_grad(p,*batch,grad=False);p[k][index]=old
  assert abs((hi-lo)/(2*eps)-g[k][index])<1e-7,(k,index,g[k][index],(hi-lo)/(2*eps))
p,vocab=d.load_best(root/'model/dialogue/dialogue2.npz');q=d.quantize(p)
subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror','-Isrc','-shared','-fPIC','src/dialogue.c','-o','build/dialogue.so'],cwd=root,check=True)
lib=ctypes.CDLL(str(root/'build/dialogue.so'));u16=ctypes.POINTER(ctypes.c_uint16);i32=ctypes.POINTER(ctypes.c_int32)
lib.holly_dialogue_logits.argtypes=[u16,ctypes.c_uint,u16,ctypes.c_uint,u16,i32]
lib.holly_dialogue_generate.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_char_p,ctypes.c_uint]
for _ in range(40):
 x=rng.integers(0,len(vocab),32,dtype=np.uint16);c=rng.integers(0,len(vocab),48,dtype=np.uint16);w=rng.integers(0,len(vocab),8,dtype=np.uint16);out=np.zeros(len(vocab),dtype=np.int32)
 lib.holly_dialogue_logits(x.ctypes.data_as(u16),32,c.ctypes.data_as(u16),48,w.ctypes.data_as(u16),out.ctypes.data_as(i32))
 assert np.array_equal(out,d.integer_logits(q,x,c,w))
matched=0
for r in rows:
 if r['split']=='train':continue
 out=ctypes.create_string_buffer(256)
 rc=lib.holly_dialogue_generate(r['user'].encode(),r['previous'].encode(),out,256)
 expected=d.generate(q,vocab,r['user'],r['previous'])
 assert rc==0 and out.value.decode()==expected,(r['user'],rc,out.value,expected)
 matched+=1
for prompt in [b'',b'\xff',b'a'*641,b'qzx qzx qzx']:
 out=ctypes.create_string_buffer(256);assert lib.holly_dialogue_generate(prompt,b'',out,256)!=0 and not out.value
out=ctypes.create_string_buffer(b'X'*5);assert lib.holly_dialogue_generate(b'how are you',b'',out,5)!=0 and not out.value
assert lib.holly_dialogue_generate(None,b'',out,5)==-1
print('Dialogue gradients: 20 finite differences; integer logits: 40 exact C/Python cases; generation:',matched,'exact parity cases; malformed/unknown/output bounds passed')
