"""Lossless row RLE of RGB565 portraits; decode one scanline at a time.

Queeg's six supplied frames map onto the existing seven mouth shapes; the
sixth frame is reused for the tongue/teeth shape. No new face is invented.
"""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
for name,w,h,file in [('holly',512,288,'hdmi_face_art.h'),('hilly',384,216,'hilly_face_art.h'),('queeg',384,216,'queeg_face_art.h')]:
    rows=[];runs=[]
    mapping=[1,2,3,4,5,6,6] if name=='queeg' else range(1,8)
    for frame in mapping:
        im=Image.open(root/f'assets/{name}-mouth/{name}{frame}.png').convert('RGB').resize((w,h),Image.Resampling.LANCZOS)
        values=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.getdata()]
        for y in range(h):
            rows.append(len(runs));row=values[y*w:(y+1)*w];at=0
            while at<w:
                end=at+1
                while end<w and row[end]==row[at]:end+=1
                runs.extend([end-at,row[at]]);at=end
    text='/* Generated lossless RGB565 row runs by export_portraits.py. */\n#include <stdint.h>\n'
    text+=f'#define {name.upper()}_PORTRAIT_WIDTH {w}u\n#define {name.upper()}_PORTRAIT_HEIGHT {h}u\n'
    if name=='holly':text+='#define HOLLY_PORTRAIT_FRAMES 7u\n'
    for suffix,data,type_ in [('rows',rows,'uint32_t'),('runs',runs,'uint16_t')]:
        text+=f'static const {type_} {name}_{suffix}[]={{\n'+''.join(','.join(str(v) for v in data[i:i+32])+',\n' for i in range(0,len(data),32))+'};\n'
    (root/'src'/file).write_text(text)
    print(name,len(rows)*4+len(runs)*2,'bytes, raw',w*h*14)
