#!/usr/bin/env python3
"""Pack user-supplied episode text for Holly's bounded persistent document bank."""
import argparse
from html.parser import HTMLParser
import json
import re
import tempfile
from pathlib import Path
from urllib.parse import urljoin,urlparse
from urllib.request import Request,urlopen
from holly_upload import HollySSH, LIMIT, prepare, upload, capabilities

PREFIX='@@EPISODE '
TITLE='RD Scripts '
SITE='https://www.ladyofthecake.com/reddwarf/html/scripts.html'

class ScriptIndex(HTMLParser):
    def __init__(self):super().__init__();self.href=None;self.links=[]
    def handle_starttag(self,tag,attrs):
        if tag=='a':self.href=dict(attrs).get('href')
    def handle_data(self,data):
        if self.href and data.strip():
            link=urljoin(SITE,self.href)
            target=urlparse(link)
            if target.hostname in ('www.ladyofthecake.com','ladyofthecake.com') and target.path.startswith('/rdscripts/') and target.path.endswith('.txt'):
                self.links.append((data.strip(),target._replace(scheme='https').geturl()))
    def handle_endtag(self,tag):
        if tag=='a':self.href=None

def fetch_site(destination):
    headers={'User-Agent':'HollyLearningOS-ScriptImporter/0.44 (+personal archive)'}
    def read(url,limit):
        with urlopen(Request(url,headers=headers),timeout=30) as response:
            target=urlparse(response.geturl())
            if target.scheme!='https' or target.hostname not in ('www.ladyofthecake.com','ladyofthecake.com'):
                raise ValueError('Script site redirected outside the selected host')
            data=response.read(limit+1)
            if len(data)>limit:raise ValueError('Script page exceeds size limit')
            return data
    index=ScriptIndex();index.feed(read(SITE,256*1024).decode('utf-8','replace'))
    if not index.links:raise ValueError('No script links at the supplied index')
    seen=set();entries=[]
    for title,url in index.links:
        if url in seen:continue
        seen.add(url)
        if not 1<=len(title)<=80 or not title.isascii():raise ValueError('Unexpected episode title')
        print(f'Downloading transcript: {title}',flush=True)
        body=read(url,256*1024)
        path=destination/f'{len(entries)+1:03d}.txt';path.write_bytes(body)
        entries.append((title,path))
    return entries

def source_files(directory,manifest=None):
    root=directory.resolve()
    if manifest:
        records=json.loads(manifest.read_text(encoding='utf-8'))
        if not isinstance(records,list) or not records:raise ValueError('Manifest must be a nonempty JSON list')
        entries=[]
        for item in records:
            if not isinstance(item,dict) or set(item)!={'title','file'}:raise ValueError('Each manifest entry needs title and file')
            path=(root/item['file']).resolve()
            if root not in path.parents or path.suffix.lower()!='.txt':raise ValueError('Manifest files must be .txt inside the input folder')
            entries.append((item['title'],path))
    else:entries=[(p.stem,p) for p in sorted(root.glob('*.txt'))]
    if not entries:raise ValueError('No episode .txt files found')
    seen=set()
    for title,path in entries:
        if not path.is_file() or not isinstance(title,str) or not 1<=len(title)<=80 or not all(32<=ord(c)<127 for c in title) or '|' in title:
            raise ValueError('Episode titles must be 1-80 printable ASCII characters and files must exist')
        if title.casefold() in seen:raise ValueError(f'Duplicate episode title: {title}')
        seen.add(title.casefold())
    return entries

def pack(entries,normalize=False):
    pieces=[];out=bytearray();count=0;total=0
    def flush():
        nonlocal out
        if out:pieces.append(bytes(out));out=bytearray()
    for title,path in entries:
        data=prepare(path,normalize)
        # Preserve text; omit blank lines and trim indentation to leave room for more episodes.
        lines=[line.strip() for line in data.split(b'\n') if line.strip()]
        lines=[b' '+line if line.startswith(PREFIX.encode('ascii')) else line for line in lines]
        marker=(PREFIX+title+'\n').encode('ascii')
        if len(marker)>=LIMIT:raise ValueError('Episode title too long')
        if not lines:raise ValueError(f'Empty episode: {title}')
        total+=len(data);count+=1
        for index,line in enumerate(lines):
            if len(line)+len(marker)+1>LIMIT:raise ValueError(f'Line exceeds a document slot in {title}')
            if not out or len(out)+len(line)+1+(len(marker) if index==0 else 0)>LIMIT:
                flush();out.extend(marker)
            # Each pack needs a marker whenever a new episode begins.
            elif index==0:out.extend(marker)
            out.extend(line+b'\n')
    flush()
    if len(pieces)>4096:raise ValueError('Collection exceeds the catalog limit; split the collection')
    return pieces,count,total

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('host',help='Holly LAN address; ignored for --dry-run')
    ap.add_argument('directory',type=Path,nargs='?',help='Folder of one .txt file per episode')
    ap.add_argument('--user',default='Rimmer',help='SSH username (current release: Rimmer)')
    ap.add_argument('--collection',default='',help='Short label for extra scripts; keep identical on resume (e.g. Extras)')
    ap.add_argument('--port',type=int,default=22)
    ap.add_argument('--from-site',action='store_true',help='Privately download the linked transcripts from the supplied index; scripts are never bundled')
    ap.add_argument('--manifest',type=Path,help='Optional JSON list of {title,file} records in episode order')
    ap.add_argument('--normalize',action='store_true',help='Transliterate a copy of Unicode script text')
    ap.add_argument('--dry-run',action='store_true',help='Validate and measure without connecting or writing')
    ap.add_argument('--ssh-option',action='append',default=[])
    args=ap.parse_args();client=None
    if args.collection and not re.fullmatch(r'[A-Za-z0-9_-]{1,24}',args.collection):ap.error('--collection must be 1-24 letters, digits, underscores or hyphens')
    pack_prefix='RD Scripts '+(args.collection+' ' if args.collection else '')
    try:
        if args.from_site:
            if args.directory or args.manifest:raise ValueError('Use --from-site without a local folder or manifest')
            temporary=tempfile.TemporaryDirectory(prefix='holly-scripts-')
            entries=fetch_site(Path(temporary.name));print(f'Index provided {len(entries)} linked transcripts (series 1-8). Later episodes and unlinked parts are not included.')
        else:
            if not args.directory:raise ValueError('Provide a folder or use --from-site')
            entries=source_files(args.directory,args.manifest)
        packs,count,total=pack(entries,args.normalize)
        print(f'{count} episode files, {total} input bytes; {len(packs)} SD document slots after packing ({sum(map(len,packs))} bytes).')
        for i,body in enumerate(packs,1):print(f'  {pack_prefix}{i:02d}: {len(body)} bytes')
        if args.dry_run:return
        client=HollySSH(args.host,user=args.user,port=args.port,options=args.ssh_option)
        slots,limit,chunk=capabilities(client)
        if any(len(body)>limit for body in packs):raise RuntimeError('Server slots are smaller than these packs; update Holly to v0.49.15 and run doc format first')
        if len(packs)>slots:raise RuntimeError('Not enough slots in this bank')
        for i,body in enumerate(packs,1):upload(client,body,f'{pack_prefix}{i:02d}')
        print('All packs committed. Ask Holly: script list; script ask The End | <scene keywords>.')
    except (OSError,ValueError,RuntimeError,KeyboardInterrupt) as exc:ap.exit(1,f'Script import stopped: {exc}\nRerun to resume verified document chunks.\n')
    finally:
        if client:client.close()
        if args.from_site and 'temporary' in locals():temporary.cleanup()
if __name__=='__main__':main()
