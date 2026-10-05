import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
page=(root/'web/index.html').read_text().encode('ascii','xmlcharrefreplace').decode('ascii')
(root/'src/http_page.h').write_text('/* Generated from web/index.html by tools/export_http.py. */\nstatic const char http_page[]=\n'+json.dumps(page)+';\n')
