"""Original larger bounded word-level conditional language model, using NumPy only.
Two position-weighted input encoders and twelve generated-word embeddings feed a clipped MLP.
Standard mathematical operations; no pretrained weights or AI framework.
"""
import argparse, hashlib, json, os, re, tempfile
from pathlib import Path
import numpy as np
E,H,W,P,C,S=80,640,12,48,64,4096
KEYS=('embedding','hidden','hidden_bias','output','output_bias')
def tokens(text):
    if len(text)>640 or any(ord(c)<32 or ord(c)>126 for c in text):raise ValueError('Use printable ASCII, at most 640 characters')
    return re.findall(r'[a-z0-9]+|[^a-z0-9\s]',text.lower())
def vocabulary(rows):
    return ['<pad>','<end>','<unknown>','<start>']+sorted({t for r in rows if r['split']=='train' for k in ('previous','user','assistant') for t in tokens(r[k])})
def encode(text,vocab,limit):
    lookup={t:i for i,t in enumerate(vocab)}
    return [lookup.get(t,2) for t in tokens(text)[-limit:]]
def read_rows(path):
    rows=[json.loads(s) for s in Path(path).read_text().splitlines() if s.strip()]
    seen=set()
    for r in rows:
        if r['split'] not in ('train','valid','test'):raise ValueError('Invalid split')
        key=(tuple(tokens(r['previous'])),tuple(tokens(r['user'])))
        if key in seen:raise ValueError('Duplicate input/context across dataset')
        seen.add(key)
        if len(r['user'])>319 or len(r['assistant'])>255 or len(tokens(r['assistant']))>31:raise ValueError('Dialogue exceeds Pi input/reply bounds')
        if not tokens(r['user']) or not tokens(r['assistant']):raise ValueError('Empty dialogue')
    if not all(any(r['split']==s for r in rows) for s in ('train','valid','test')):raise ValueError('All three splits required')
    return rows

def dataset(rows,vocab,split):
    xx=[];cc=[];ww=[];yy=[]
    selected=[]
    contexts=[r['user']+' '+r['assistant'] for r in rows if r['split']=='train' and not r['previous']]
    for index,r in enumerate(rows):
        if r['split']!=split:continue
        selected.append(r)
        if split=='train' and not r['family'].startswith(('reference:','alias:','actor_context:')):selected.extend([r]*3)
        if split=='train' and not r['previous']:
            for offset in (7,43):
                selected.append(dict(r,previous=contexts[(index+offset)%len(contexts)]))
    for r in selected:
        x=encode(r['user'],vocab,P);c=encode(r['previous'],vocab,C)
        x=x+[0]*(P-len(x));c=c+[0]*(C-len(c));w=[0]*(W-1)+[3]
        for y in encode(r['assistant'],vocab,48)+[1]:
            xx.append(x);cc.append(c);ww.append(w.copy());yy.append(y);w=w[1:]+[y]
    return tuple(np.array(a,dtype=np.int32) for a in (xx,cc,ww,yy))
def init(vocab,seed=37):
    rng=np.random.default_rng(seed);v=len(vocab)
    p={'embedding':rng.normal(0,.2,(v,E)), 'hidden':rng.normal(0,1/np.sqrt((W+2)*E),((W+2)*E,H)),
       'hidden_bias':np.zeros(H),'output':rng.normal(0,1/np.sqrt(H),(H,v)),'output_bias':np.zeros(v)}
    return {k:a.astype(np.float32) for k,a in p.items()}
def loss_grad(p,x,c,w,y,grad=True):
    # Order-sensitive weighted pooling: the last words carry more of the
    # question, unlike Dialogue-3's bag-of-words average.
    xm=(x!=0)[...,None]*np.arange(1,x.shape[1]+1)[None,:,None]
    cm=(c!=0)[...,None]*np.arange(1,c.shape[1]+1)[None,:,None]
    xn=np.maximum(1,xm.sum(1));cn=np.maximum(1,cm.sum(1))
    a=(p['embedding'][x]*xm).sum(1)/xn;b=(p['embedding'][c]*cm).sum(1)/cn
    z=np.concatenate((a,b,p['embedding'][w].reshape(len(x),-1)),1)
    pre=z@p['hidden']+p['hidden_bias'];h=np.clip(pre,-1,1)
    logits=h@p['output']+p['output_bias'];logits-=logits.max(1,keepdims=True)
    probs=np.exp(logits);probs/=probs.sum(1,keepdims=True)
    loss=float(-np.log(np.maximum(probs[np.arange(len(y)),y],1e-30)).mean())
    if not grad:return loss
    d=probs;d[np.arange(len(y)),y]-=1;d/=len(y)
    dh=(d@p['output'].T)*(abs(pre)<1);dz=dh@p['hidden'].T
    g={'output':h.T@d,'output_bias':d.sum(0),'hidden':z.T@dh,'hidden_bias':dh.sum(0),'embedding':np.zeros_like(p['embedding'])}
    np.add.at(g['embedding'],x,(dz[:,:E]/xn)[:,None,:]*xm)
    np.add.at(g['embedding'],c,(dz[:,E:2*E]/cn)[:,None,:]*cm)
    np.add.at(g['embedding'],w,dz[:,2*E:].reshape(len(x),W,E))
    return loss,g

def evaluate(p,data):
    return sum(loss_grad(p,*(a[i:i+128] for a in data),grad=False)*len(data[3][i:i+128]) for i in range(0,len(data[3]),128))/len(data[3])
def quantize(p):
    if any(not np.isfinite(a).all() for a in p.values()):raise ValueError('Non-finite weights')
    q={k:np.rint(a*S).astype(np.int64) for k,a in p.items()}
    if any(abs(a).max()>32767 for a in q.values()):raise ValueError('Weights exceed int16')
    return q

def trunc_div(a,b):return np.where(a<0,-((-a)//b),a//b)
def integer_logits(q,x,c,w):
    def pool(ids):
        ids=np.asarray(ids,dtype=np.int64)
        weights=np.arange(1,len(ids)+1,dtype=np.int64)*(ids!=0)
        if not weights.sum():return np.zeros(E,dtype=np.int64)
        return trunc_div((q['embedding'][ids]*weights[:,None]).sum(0),int(weights.sum()))
    z=np.concatenate((pool(x),pool(c),q['embedding'][w].reshape(-1)))
    h=np.clip(trunc_div(z@q['hidden'],S)+q['hidden_bias'],-S,S)
    return trunc_div(h@q['output'],S)+q['output_bias']
def detokenize(words):
    out=''
    for t in words:
        if t in ('.',',','?','!',':',';'):out=out.rstrip()+t
        elif t=="'":out=out.rstrip()+t
        elif out.endswith("'"):out+=t
        elif t=='=':out+=' ='
        elif t=='>':out+='>'
        else:out+=(' ' if out else '')+t
    out=re.sub(r'(^|[.!?] )([a-z])',lambda m:m[1]+m[2].upper(),out)
    out=re.sub(r'\bi\b','I',out)
    return out

def generate(q,vocab,prompt,previous=''):
    x=encode(prompt,vocab,P);c=encode(previous,vocab,C);w=[0]*(W-1)+[3];out=[]
    for _ in range(32):
        scores=integer_logits(q,x,c,w);scores[[0,2,3]]=-2**60
        y=int(scores.argmax())
        if y==1:break
        out.append(vocab[y]);w=w[1:]+[y]
    return detokenize(out)

def atomic_npz(path,arrays):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    fd,name=tempfile.mkstemp(dir=path.parent,suffix='.npz');os.close(fd)
    try:
        np.savez_compressed(name,**arrays);os.replace(name,path)
    finally:
        if os.path.exists(name):os.unlink(name)

def train(args):
    if args.steps<1 or args.batch<1 or args.batch>512 or not 0<args.lr<=.1:raise ValueError("Invalid training controls")
    rows=read_rows(args.data);vocab=vocabulary(rows)
    if len(vocab)>1024:raise ValueError('Vocabulary limit 1024')
    datahash=hashlib.sha256(Path(args.data).read_bytes()).hexdigest()
    rng=np.random.default_rng(37);p=init(vocab);m={k:np.zeros_like(a) for k,a in p.items()};v={k:a.copy() for k,a in m.items()};step=0
    tr=dataset(rows,vocab,'train');va=dataset(rows,vocab,'valid');best=evaluate(p,va);bestp={k:a.copy() for k,a in p.items()}
    if args.resume:
        with np.load(args.resume,allow_pickle=False) as z:
            meta=json.loads(str(z['metadata']))
            if meta['datahash']!=datahash or list(z['vocab'])!=vocab:raise ValueError('Resume requires identical dataset and vocabulary')
            for k in KEYS:
                for target,prefix in ((p,''),(m,'m_'),(v,'v_'),(bestp,'best_')):
                    a=z[prefix+k]
                    if a.shape!=target[k].shape or not np.isfinite(a).all():raise ValueError('Invalid checkpoint')
                    target[k]=a.copy()
            rng.bit_generator.state=meta['rng'];step=meta['step'];best=meta['best']
    def save():
        arrays={k:a for k,a in p.items()}
        for source,prefix in ((m,'m_'),(v,'v_'),(bestp,'best_')):arrays.update({prefix+k:a for k,a in source.items()})
        arrays.update(vocab=np.array(vocab),metadata=np.array(json.dumps(dict(datahash=datahash,rng=rng.bit_generator.state,step=step,best=best))))
        atomic_npz(args.output,arrays)
    for update in range(args.steps):
        step+=1;idx=rng.integers(0,len(tr[3]),args.batch)
        loss,g=loss_grad(p,*(a[idx] for a in tr))
        norm=np.sqrt(sum(float((a*a).sum()) for a in g.values()));clip=min(1,5/max(norm,1e-10))
        for k in KEYS:
            grad=g[k]*clip;m[k]=.9*m[k]+.1*grad;v[k]=.999*v[k]+.001*grad*grad
            p[k]-=args.lr*(m[k]/(1-.9**step))/(np.sqrt(v[k]/(1-.999**step))+1e-8)
        if step%200==0 or update+1==args.steps:
            val=evaluate(p,va)
            if val<best:best=val;bestp={k:a.copy() for k,a in p.items()}
            save();print(f'step {step} train_batch {loss:.4f} valid {val:.4f} best {best:.4f}',flush=True)
    report(bestp,vocab,rows,args.output+'.report.json')
def load_best(path):
    with np.load(path,allow_pickle=False) as z:return {k:z['best_'+k].copy() for k in KEYS},list(z['vocab'])
def report(p,vocab,rows,path):
    q=quantize(p);r={'parameters':sum(a.size for a in p.values()),'vocabulary':len(vocab),'splits':{},'samples':[]}
    for split in ('train','valid','test'):
        examples=[x for x in rows if x['split']==split];correct=0
        for x in examples:
            got=generate(q,vocab,x['user'],x['previous']);correct+=tokens(got)==tokens(x['assistant'])
            if split!='train':r['samples'].append(dict(split=split,user=x['user'],previous=x['previous'],expected=x['assistant'],generated=got))
        r['splits'][split]={'examples':len(examples),'exact_replies':correct,'loss':evaluate(p,dataset(rows,vocab,split))}
    Path(path).write_text(json.dumps(r,indent=2));print(json.dumps({k:v for k,v in r.items() if k!='samples'}),flush=True)
def export(args):
    p,vocab=load_best(args.checkpoint);q=quantize(p)
    lines=['/* Generated from original Dialogue-4 weights. No pretrained model. */','#ifndef HOLLY_DIALOGUE_WEIGHTS_H','#define HOLLY_DIALOGUE_WEIGHTS_H','#include <stdint.h>',f'#define DL_VOCAB {len(vocab)}',f'#define DL_PARAMETERS {sum(a.size for a in p.values())}']
    lines.append('static const char *const dl_vocab[DL_VOCAB]={'+','.join(json.dumps(t) for t in vocab)+'};')
    for k,a in q.items():lines.append('static const int16_t dl_'+k+'['+str(a.size)+']={'+','.join(str(n) for n in a.ravel())+'};')
    lines.append('#endif');Path(args.output).write_text('\n'.join(lines)+'\n')
if __name__=='__main__':
    ap=argparse.ArgumentParser();sub=ap.add_subparsers(dest='command',required=True)
    t=sub.add_parser('train');t.add_argument('--data',default=str(Path(__file__).with_name('corpus4.jsonl')));t.add_argument('--output',default='dialogue4.npz');t.add_argument('--resume');t.add_argument('--steps',type=int,default=1200);t.add_argument('--batch',type=int,default=64);t.add_argument('--lr',type=float,default=.003)
    e=sub.add_parser('export');e.add_argument('checkpoint');e.add_argument('output')
    g=sub.add_parser('generate');g.add_argument('checkpoint');g.add_argument('prompt');g.add_argument('--previous',default='')
    a=ap.parse_args()
    if a.command=='train':train(a)
    elif a.command=='export':export(a)
    else:
        p,v=load_best(a.checkpoint);print(generate(quantize(p),v,a.prompt,a.previous))
