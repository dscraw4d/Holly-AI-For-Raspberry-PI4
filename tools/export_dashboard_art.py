#!/usr/bin/env python3
"""Encode supplied PNG for original kernel HTTP and RGB565 HDMI renderer."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
source=root/'web/holly-reference.png'
b=source.read_bytes()
assert len(b)+768<524288, 'Artwork exceeds the bounded TCP response queue'
(root/'src/http_art.h').write_text('/* Original supplied PNG, served losslessly. */\nstatic const unsigned char http_art[]={\n'+','.join(str(x) for x in b)+'\n};\n')
im=Image.open(source).convert('RGB').resize((640,360),Image.Resampling.LANCZOS)
values=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.getdata()]
(root/'src/dashboard_art.h').write_text('/* Supplied reference, resampled to 640 x 360 RGB565. */\nstatic const uint16_t dashboard_art[]={\n'+','.join(hex(x) for x in values)+'\n};\n')
print('Encoded original PNG and HDMI artwork')
