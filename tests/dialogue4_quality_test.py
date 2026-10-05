"""Compare exactly the same held-out prompts and check practical regressions.
This is a paraphrase benchmark, not a test of broad conversational intelligence.
"""
import importlib.util,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
models={}
for n in (3,4):
 spec=importlib.util.spec_from_file_location(f'dialogue{n}',ROOT/f'model/dialogue/dialogue{n}.py')
 m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 p,v=m.load_best(ROOT/f'model/dialogue/dialogue{n}.npz');models[n]=(m,m.quantize(p),v)
original=[json.loads(x) for x in (ROOT/'model/dialogue/corpus3.jsonl').read_text().splitlines()]
current=[json.loads(x) for x in (ROOT/'model/dialogue/corpus4.jsonl').read_text().splitlines()]
held=[r for r in original if r['split']=='test']
report={'same_test_examples':len(held),'models':{},'regressions':[],'novel_probes':[]}
for n,(m,q,v) in models.items():
 exact=sum(m.tokens(m.generate(q,v,r['user'],r['previous']))==m.tokens(r['assistant']) for r in held)
 report['models'][str(n)]={'exact':exact}
m,q,v=models[4]
probes=[('hello',''),('how are you',''),('tell me about rimmer',''),('tell me about ace rimmer',''),('who plays them','We are discussing Arnold Rimmer.')]
for user,previous in probes:
 row=next(r for r in current if r['user'].lower()==user.lower() and r['previous']==previous)
 generated=m.generate(q,v,user,previous)
 exact=m.tokens(generated)==m.tokens(row['assistant'])
 low=generated.lower();words=m.tokens(generated)
 practical=exact
 if user=='hello':practical=('?' in generated and any(w in words for w in ['hello','ready','chat']) and words.count('hello')<=2 and len(words)<40)
 if user=='who plays them':practical=('chris barrie' in low and 'rimmer' in low and not any(name in low for name in ['charles','llewellyn','hayridge']))
 report['regressions'].append({'user':user,'previous':previous,'expected':row['assistant'],'generated':generated,'exact':exact,'passed':practical})
for user in ['what do you think of lister','why does kryten obey rimmer','how would holly resolve an argument between the crew']:
 report['novel_probes'].append({'user':user,'generated':m.generate(q,v,user)})
(ROOT/'release-evidence/v0.48-model-comparison.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
assert report['models']['4']['exact']>=report['models']['3']['exact'],'Held-out replies regressed'
assert all(r['passed'] for r in report['regressions']),'Practical dialogue regressions remain'
