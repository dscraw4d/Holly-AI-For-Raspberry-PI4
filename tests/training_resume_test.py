import hashlib
from pathlib import Path
import sys
import tempfile
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'model'))
import holly_lm as lm
p=lm.initialize();m={k:np.zeros_like(a) for k,a in p.items()};v={k:a.copy() for k,a in m.items()};rng=np.random.default_rng(43)
_,x,y=lm.examples(Path(__file__).resolve().parents[1]/'model/data/train.txt')
for step in range(1,5):lm.adam_step(p,m,v,rng,x,y,step)
with tempfile.TemporaryDirectory() as d:
    path=Path(d)/'resume.npz'
    lm.save_training(path,p,m,v,p,4,4,rng,'train','valid')
    for step in range(5,9):lm.adam_step(p,m,v,rng,x,y,step)
    rp,rm,rv,best,meta,rrng=lm.restore_training(path)
    for step in range(5,9):lm.adam_step(rp,rm,rv,rrng,x,y,step)
    for a,b in [(p,rp),(m,rm),(v,rv)]:
        for k in a:assert np.array_equal(a[k],b[k]),k
    assert rng.bit_generator.state==rrng.bit_generator.state
    assert lm.load(path)['embedding'].shape==lm.SHAPES['embedding']
print('Desktop resume: weights, Adam moments and RNG match uninterrupted training exactly')
