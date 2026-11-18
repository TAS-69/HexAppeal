from pathlib import Path
import json,xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
from development_paths import SOURCES
from witch_parts import PRIVATE_BOOTS
manifest=json.loads((root/'build-data'/'witch-assets.json').read_text(encoding="utf-8"))
paths=sorted({t['appearance'] for t in manifest})
for path in paths:
    source=SOURCES/'vanilla'/'appearances'/'0009'/path
    doc=ET.fromstring(source.read_bytes())
    nude=doc.findall('./Nude/Prefab')
    assert len(nude)==1,path
    original=nude[0].get('Name')
    assert original and len([p for p in doc.iter('Prefab') if p.get('Name')==original])==1,path
    dest=root/'Hex Appeal - Bodies'/'files'/'0009'/(path+'.patch')
    dest.parent.mkdir(exist_ok=True,parents=True)
    operations=[ET.Element('set',{'find':'Prefab','key':original,'attr':'Name','value':'phw_00_nude_00_0001_damian'})]
    for part in doc.iter('Prefab'):
        name=part.get('Name')
        if name in PRIVATE_BOOTS:operations.append(ET.Element('set',{'find':'Prefab','key':name,'attr':'Name','value':PRIVATE_BOOTS[name]}))
    dest.write_text('<xml-patch>\n'+''.join('  '+ET.tostring(operation,encoding='unicode')+'\n' for operation in operations)+'</xml-patch>\n',encoding='utf-8')
print('Built',len(paths),'body appearance patches. VENUS supplies the body model and physics.')
