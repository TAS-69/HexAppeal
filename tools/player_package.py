from pathlib import PurePosixPath
import hashlib,json

def player_package(version,body_files,refit_files,runtime_files,documents,allowed_pacs,dependency_hashes):
    entries={}
    reserved=set()
    def add(name,data):
        path=PurePosixPath(name)
        if not name or path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
            raise ValueError('Invalid package path: '+name)
        key=name.casefold()
        if key in reserved:raise ValueError('Duplicate package path: '+name)
        digest=hashlib.sha256(data).hexdigest()
        if digest in dependency_hashes:raise ValueError('External dependency content cannot be bundled: '+name)
        suffix=path.suffix.lower()
        if suffix in {'.exe','.dll','.obj','.pdb','.lib','.exp','.pyc','.dds','.hkx','.pac_xml','.app_xml'}:
            raise ValueError('Unexpected package binary: '+name)
        if suffix=='.asi' and name!='HexAppeal.asi':raise ValueError('Only the Hex Appeal runtime belongs in this package.')
        if suffix=='.pac' and allowed_pacs.get(name)!=digest:raise ValueError('Unexpected model: '+name)
        reserved.add(key);entries[name]=data
    metadata={'modinfo':{'title':'Hex Appeal','version':version,'author':'TAS-69',
                        'description':'VENUS witch bodies, default outfit refits, the F9 editor and Wardrobe in one package. Requires the separately installed dependencies.'}}
    add('mod.json',(json.dumps(metadata,indent=2)+'\n').encode('utf-8'))
    add('HEX-APPEAL-PACKAGE.txt',b'HexAppeal.PlayerPackage/1\n')
    for name,data in [*body_files,*refit_files]:
        if not name.startswith('files/'):raise ValueError('Game assets must use the files directory.')
        add(name,data)
    for name,data in runtime_files:
        if name not in {'HexAppeal.asi','HexAppeal.ini'}:raise ValueError('Unexpected runtime entry: '+name)
        add(name,data)
    for name,data in documents:add(name,data)
    if not {'HexAppeal.asi','HexAppeal.ini'}<=entries.keys():raise ValueError('The runtime and its configuration are both required.')
    if {name for name in entries if name.endswith('.pac')}!=allowed_pacs.keys():raise ValueError('Default garment output set is incomplete.')
    return [('Hex Appeal/'+name,data) for name,data in sorted(entries.items())]
