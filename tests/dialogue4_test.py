"""New model: original gradients, integer parity, bounded generation, held-out gate."""
import ctypes,importlib.util,json,subprocess
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('d3',root/'model/dialogue/dialogue4.py')
d=importlib.util.module_from_spec(spec);spec.loader.exec_module(d)
rows=d.read_rows(root/'model/dialogue/corpus4.jsonl');vocab=d.vocabulary(rows)
p=d.init(vocab);p={k:a.astype(np.float64) for k,a in p.items()};data=d.dataset(rows,vocab,'train');batch=tuple(a[:2] for a in data)
loss,grad=d.loss_grad(p,*batch);rng=np.random.default_rng(31)
for k in d.KEYS:
 for _ in range(2):
  idx=tuple(int(rng.integers(n)) for n in p[k].shape);old=p[k][idx];eps=1e-5
  p[k][idx]=old+eps;hi=d.loss_grad(p,*batch,grad=False)
  p[k][idx]=old-eps;lo=d.loss_grad(p,*batch,grad=False);p[k][idx]=old
  assert abs((hi-lo)/(2*eps)-grad[k][idx])<1e-7,(k,idx)
p,v=d.load_best(root/'model/dialogue/dialogue4.npz');q=d.quantize(p)
subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror','-Isrc','-shared','-fPIC','src/dialogue4.c','-o','build/dialogue4.so'],cwd=root,check=True)
lib=ctypes.CDLL(str(root/'build/dialogue4.so'));u16=ctypes.POINTER(ctypes.c_uint16);i32=ctypes.POINTER(ctypes.c_int32)
lib.holly_dialogue4_logits.argtypes=[u16,ctypes.c_uint,u16,ctypes.c_uint,u16,i32]
lib.holly_dialogue4_generate.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_char_p,ctypes.c_uint]
for _ in range(40):
 x=rng.integers(0,len(v),48,dtype=np.uint16);c=rng.integers(0,len(v),64,dtype=np.uint16);w=rng.integers(0,len(v),12,dtype=np.uint16);out=np.zeros(len(v),dtype=np.int32)
 lib.holly_dialogue4_logits(x.ctypes.data_as(u16),48,c.ctypes.data_as(u16),64,w.ctypes.data_as(u16),out.ctypes.data_as(i32))
 assert np.array_equal(out,d.integer_logits(q,x,c,w))
for r in rows:
 if r['split']=='train':continue
 out=ctypes.create_string_buffer(256)
 rc=lib.holly_dialogue4_generate(r['user'].encode(),r['previous'].encode(),out,256)
 expected=d.generate(q,v,r['user'],r['previous'])
 assert rc==0 and out.value.decode()==expected,(r['user'],rc,out.value,expected)
for prompt in [b'',b'\xff',b'a'*641,b'qzx qzx qzx']:
 out=ctypes.create_string_buffer(256);assert lib.holly_dialogue4_generate(prompt,b'',out,256)!=0 and not out.value
for cap in (1,5):
 out=ctypes.create_string_buffer(cap);assert lib.holly_dialogue4_generate(b'what is lister',b'',out,cap)!=0 and not out.value
report=json.loads((root/'model/dialogue/dialogue4.npz.report.json').read_text())
assert report['parameters']>1000000 and report['splits']['test']['exact_replies']>=65
print(f'Dialogue-4: 10 numerical gradients, 40 exact integer logits, {sum(r["split"] != "train" for r in rows)} C/Python reply parity cases, invalid input and output bounds passed')
