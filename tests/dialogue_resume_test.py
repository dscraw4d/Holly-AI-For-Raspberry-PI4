import argparse,importlib.util,json,tempfile
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('dialogue',root/'model/dialogue/dialogue.py');d=importlib.util.module_from_spec(spec);spec.loader.exec_module(d)
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)
 def run(name,steps,resume=None):
  d.train(argparse.Namespace(data=str(root/'model/dialogue/corpus.jsonl'),output=str(p/name),resume=str(p/resume) if resume else None,steps=steps,batch=16,lr=.003))
 run('full.npz',20);run('part.npz',10);run('resume.npz',10,'part.npz')
 with np.load(p/'full.npz',allow_pickle=False) as a,np.load(p/'resume.npz',allow_pickle=False) as b:
  for k in d.KEYS:
   for prefix in ('','m_','v_'):assert np.array_equal(a[prefix+k],b[prefix+k]),(prefix,k)
  x=json.loads(str(a['metadata']));y=json.loads(str(b['metadata']));assert x['step']==y['step']==20 and x['rng']==y['rng']
 print('Dialogue training resumes with identical current weights, optimizer moments and RNG state')
