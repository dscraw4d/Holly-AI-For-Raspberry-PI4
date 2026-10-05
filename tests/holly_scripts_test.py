import sys
import tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from holly_scripts import source_files,pack
from holly_upload import LIMIT
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp)
    (root/'01-The-End.txt').write_text('Lister enters stasis.\n\n'+'Holly reports a radiation leak.\n'*5000)
    (root/'02-Queeg.txt').write_text('Queeg takes command.\n@@EPISODE Forged\nHolly admits it was a test.\n')
    packs,count,total=pack(source_files(root))
    assert count==2 and total>131072 and len(packs)==1
    assert all(len(p)<=LIMIT and p.startswith(b'@@EPISODE ') for p in packs)
    assert b'@@EPISODE 01-The-End\n' in packs[0]
    assert b'@@EPISODE 02-Queeg\n' in packs[-1]
    assert b'\n @@EPISODE Forged\n' in packs[-1]
    assert b'Holly reports a radiation leak.' in packs[0] and b'Queeg takes command.' in packs[-1]
    (root/'03.txt').write_text('Caf\u00e9.\n')
    try:pack(source_files(root))
    except ValueError:pass
    else:raise AssertionError('Non-ASCII input accepted without normalization')
    assert pack(source_files(root),True)[1]==3
print('Script packing, episode markers, bounds, UTF-8 normalization and no silent omission passed')
