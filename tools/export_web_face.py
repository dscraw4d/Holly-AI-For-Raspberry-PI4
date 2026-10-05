#!/usr/bin/env python3
"""Encode the same seven supplied HDMI portraits as one browser atlas.
One bounded request prevents parallel image downloads on the Pi HTTP stream.
"""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
width,height=512,288
atlas=Image.new('RGB',(width,height*21),'black')
for avatar_index,avatar in enumerate(['holly','hilly','queeg']):
 for frame in range(7):
  source_frame=min(frame+1,6) if avatar=='queeg' else frame+1
  im=Image.open(root/f'assets/{avatar}-mouth'/f'{avatar}{source_frame}.png').convert('RGB').resize((width,height),Image.Resampling.LANCZOS)
  atlas.paste(im,(0,height*(avatar_index*7+frame)))
p=root/'web/holly-face.jpg';atlas.save(p,quality=92,subsampling=0,optimize=True)
b=p.read_bytes();assert len(b)+1024<524288
(root/'src/http_art.h').write_text('/* Seven supplied HDMI portraits in one browser JPEG atlas. */\nstatic const unsigned char http_art[]={\n'+''.join(','.join(str(v) for v in b[at:at+32])+',\n' for at in range(0,len(b),32))+'};\n')
print('Portrait atlas:',len(b),'bytes,',atlas.size)
