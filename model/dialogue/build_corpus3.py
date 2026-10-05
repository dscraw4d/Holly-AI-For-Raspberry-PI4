"""Original curated reference phrases, not episode scripts or web scraping."""
import json,re,sys
from pathlib import Path
base=Path(__file__).resolve().parent
rows=[json.loads(line) for line in (base/'corpus.jsonl').read_text().splitlines() if line.strip()]
existing={(x['previous'],x['user'].lower()) for x in rows}
knowledge=json.loads((base.parent/'lore/knowledge.json').read_text())
phrases=['tell me about {subject}', 'describe {subject}', 'what is {subject}',
 'can we discuss {subject}', 'explain {subject}', 'what do you know about {subject}',
 'give me some background on {subject}', 'who or what is {subject}',
 'could you describe {subject}', 'please tell me about {subject}']
for topic in knowledge['topics']:
 title=topic['title']
 passage=topic['pages'][0]['text'].split('. ')[0].strip()
 if not passage.endswith('.'):passage+='.'
 if len(passage)>255 or len(re.findall(r'[a-z0-9]+|[^a-z0-9\s]',passage.lower()))>31:continue
 for i,t in enumerate(phrases):
  q=t.format(subject=title)
  key=('',q.lower())
  if key in existing:continue
  existing.add(key)
  rows.append(dict(split='train' if i<8 else 'valid' if i==8 else 'test',family='reference:'+title,previous='',user=q,assistant=passage))
(base/'corpus3.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=True)+'\n' for r in rows))
print('Authored examples:',len(rows),'topics:',sum(1 for t in knowledge['topics'] if any(x['family']=='reference:'+t['title'] for x in rows)))
