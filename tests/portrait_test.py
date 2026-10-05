"""Compare compiled portrait pixels with the resized supplied originals."""
from pathlib import Path
from PIL import Image
import re
root=Path(__file__).resolve().parents[1]
for name,w,h,header in [('holly',512,288,'hdmi_face_art.h'),('hilly',384,216,'hilly_face_art.h'),('queeg',384,216,'queeg_face_art.h')]:
    text=(root/'src'/header).read_text()
    def array(suffix):
        return [int(v) for v in re.search(name+'_'+suffix+r'\[\]=\{(.*?)\};',text,re.S).group(1).split(',') if v.strip()]
    rows,runs=array('rows'),array('runs')
    assert len(rows)==7*h
    for frame in range(7):
        source=min(frame+1,6) if name=='queeg' else frame+1
        im=Image.open(root/f'assets/{name}-mouth/{name}{source}.png').convert('RGB').resize((w,h),Image.Resampling.LANCZOS)
        expected=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.getdata()]
        for y in range(h):
            at=rows[frame*h+y];decoded=[]
            while len(decoded)<w:
                count,value=runs[at:at+2];assert 0<count<=w-len(decoded)
                decoded.extend([value]*count);at+=2
            assert decoded==expected[y*w:(y+1)*w],(name,frame,y)
    print(name+': all seven compiled RGB565 frames match supplied source pixels')
