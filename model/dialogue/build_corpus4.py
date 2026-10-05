"""Extend the authored dialogue set with short multi-turn exchanges. No scripts copied."""
import json,re
from pathlib import Path
base=Path(__file__).parent
rows=[json.loads(x) for x in (base/'corpus3.jsonl').read_text().splitlines()]
for prompt in ['hello','hi','hey','hello holly']:
 if any(r['previous']=='' and r['user']==prompt for r in rows):continue
 rows.append(dict(split='train',family='greeting',previous='',user=prompt,assistant='Hello. I am Holly. What would you like to talk about?'))
examples=[
 ('crew_opinion','I think Rimmer is difficult.','Which Rimmer scene changed your mind?',
  'Rimmer certainly gives us a lot to discuss. Which scene are you thinking of?',
  ['i think rimmer is difficult','rimmer gets on my nerves','rimmer is hard to like','rimmer acts oddly','what do you make of rimmer','i have an opinion about rimmer','can we discuss rimmer','rimmer can be frustrating','i am unsure about rimmer','tell me your view of rimmer']),
 ('crew_speculation','What if Lister commanded the ship?','That would be a different voyage.',
  'That is an alternative story, not an episode fact. What would Lister change first?',
  ['what if lister ran the ship','suppose lister was in command','imagine lister as captain','what if lister were captain','picture lister giving the orders','could lister command red dwarf','what would happen if lister was captain','would lister make a good captain','if lister led the crew what then','imagine lister leading the crew']),
 ('evidence','I think Kryten solved it.','Do we have evidence?','Let us check the episode record before treating that as canon.',
  ['can you prove that from the script','is that actually in the episode','where did you get that','can we verify it','is there a source for that','show me the evidence','are you certain it happened','how do we know it happened','was that really shown','what passage supports that']),
 ('uncertain_canon','I heard Holly built a new ship.','Is that in the show?',
  'I cannot confirm that from this conversation. Let us search the records.',
  ['did holly build a new ship','was there a new ship built by holly','is that holly story canon','are you sure about a holly ship','did the scripts say holly built it','can holly build ships in the series','does holly create a ship','show the scene where holly built a ship','which episode says holly built it','is the claim about holly building a ship real']),
 ('counterfactual','What if Rimmer never became a hologram?','Would Lister be alone?',
  'That is a hypothetical. I can discuss it without calling it a fact from the show.',
  ['imagine rimmer had lived','what if rimmer was not a hologram','suppose rimmer was still alive','what if rimmer never died','would lister have met a living rimmer','what if the radiation leak missed rimmer','can we change the rimmer story','imagine rimmer surviving the accident','would rimmer be different if alive','what if rimmer survived']),
 ('reflect','I enjoy the odd crew.','What part is best?',
  'Tell me which moment stood out and I will follow your line of thought.',
  ['i have a favourite scene','there is a moment i liked','one episode stayed with me','i want to tell you a scene','can i share my favourite bit','that reminds me of an episode','i was thinking of a scene','i liked a particular joke','i enjoyed that moment','shall i describe the scene']),
 ('followup','We were talking about the Cat.','What else?',
  'We can stay with the Cat. Do you mean his origins or a particular episode?',
  ['go on about him','tell me more about that character','what else about him','continue that subject','what happened next with him','another detail about the cat','keep talking about the cat','and what else happened','carry on about the cat','can we stay on that topic']),
 ('correction_scene','The quote may be wrong.','Check it again.',
  'Fair point. Give me the episode or passage and we can correct the record.',
  ['you got that scene wrong','that was not in the episode','i think that quote is wrong','the scene happened differently','that answer seems invented','please check the script','you mixed up two episodes','you have the wrong character','that is not what happened','can you correct that scene']),
]
for family,previous,_,answer,prompts in examples:
 for i,prompt in enumerate(prompts):
  rows.append(dict(split='train' if i<8 else 'valid' if i==8 else 'test',family=family,previous=previous,user=prompt,assistant=answer))
existing={(r['previous'].lower(),r['user'].lower()) for r in rows}
knowledge=json.loads((base.parent/'lore/knowledge.json').read_text())
for topic in knowledge['topics']:
 answer=topic['pages'][0]['text'].split('. ')[0].strip()
 if not answer.endswith('.'):answer+='.'
 if len(answer)>255 or len(re.findall(r'[a-z0-9]+|[^a-z0-9\s]',answer.lower()))>31:continue
 for alias in topic['aliases']:
  for form in ['tell me about {}','who is {}','describe {}','what is {}']:
   user=form.format(alias);key=('',user.lower())
   if key in existing:continue
   existing.add(key);rows.append(dict(split='train',family='alias:'+topic['title'],previous='',user=user,assistant=answer))
 actors=[p['text'] for p in topic['pages'] if ' plays ' in p['text'].lower() or ' play ' in p['text'].lower()]
 for answer in actors:
  if len(answer)>255 or len(re.findall(r'[a-z0-9]+|[^a-z0-9\s]',answer.lower()))>31:continue
  previous='We are discussing '+topic['title']+'.'
  forms=['who plays that character','who plays them','which actor portrays them','who is the actor','tell me the actor','who plays this role','who portrayed that role','name the performer','who is behind that character','which performer plays that part']
  for i,user in enumerate(forms):
   key=(previous.lower(),user.lower())
   if key in existing:continue
   existing.add(key);rows.append(dict(split='train' if i<8 else 'valid' if i==8 else 'test',family='actor_context:'+topic['title'],previous=previous,user=user,assistant=answer))
(base/'corpus4.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=True)+'\n' for x in rows))
print(len(rows),'authored examples with',len(examples),'new multi-turn families')
