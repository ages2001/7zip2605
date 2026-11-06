"""Convert the official English 7-Zip manual to native-viewer resources."""
from html.parser import HTMLParser
from pathlib import Path
import json
import re
import sys

class Text(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts=[]; self.title=[]; self.in_title=False; self.skip=0
    def handle_starttag(self,tag,attrs):
        if tag=='title': self.in_title=True
        if tag in ('style','script'): self.skip+=1
        if tag in ('p','div','br','li','tr','h1','h2','h3','pre','hr'): self.parts.append('\n')
        if tag=='li': self.parts.append(' * ')
        if tag in ('td','th'): self.parts.append('\t')
    def handle_endtag(self,tag):
        if tag=='title': self.in_title=False
        if tag in ('style','script'): self.skip-=1
        if tag in ('p','div','li','tr','h1','h2','h3','pre'): self.parts.append('\n')
    def handle_data(self,data):
        if self.in_title: self.title.append(data)
        elif not self.skip: self.parts.append(data)

root=Path(sys.argv[1]); output=Path(sys.argv[2]);output.mkdir(parents=True,exist_ok=True)
entries=[];resources=[]
for index,path in enumerate(sorted(root.rglob('*.htm'))):
    parsed=Text();parsed.feed(path.read_text(encoding='utf-8-sig',errors='replace'))
    title=' '.join(''.join(parsed.title).split()) or path.stem
    body=re.sub(r'\n[ \t]*\n(?:[ \t]*\n)+','\n\n',''.join(parsed.parts)).strip()
    assert len(body.encode('cp1252',errors='xmlcharrefreplace'))<60000,path
    text=output/f'h{index:03d}.txt';text.write_bytes((title+'\r\n\r\n'+body.replace('\n','\r\n')).encode('cp1252',errors='xmlcharrefreplace')+b'\0')
    resource_id=6000+index
    entries.append('{'+','.join([json.dumps(path.relative_to(root).as_posix()),json.dumps(title,ensure_ascii=True),str(resource_id)])+'}')
    resources.append(f'{resource_id} RCDATA {json.dumps(str(text))}')
assert entries,'No manual topics found'
(output/'help_topics.h').write_text('static const struct HelpTopic {const char *path; const char *title; unsigned id;} topics[]={\n'+',\n'.join(entries)+'\n};\n')
(output/'help.rc').write_text('\n'.join(resources)+'\n')
print(f'Generated {len(entries)} English help topics')
