from pathlib import PurePosixPath
import hashlib,json,posixpath,re
from urllib.parse import quote,unquote
from release_policy import validate_release_path

GAME_RESOURCES = {'.pac_xml', '.prefab', '.pappt', '.hkx', '.prefabdata_xml', '.dds'}
NEXUS_NAME = 'TASx69'
SUMMARY = 'Applies the Venus body to every witch in Crimson Desert. Because even dark magic deserves better curves.'
WARDROBE_INTERACTION_FILE = 'ha_wardrobe.field.json'

def player_package(version,body_files,refit_files,runtime_files,documents,allowed_pacs,dependency_hashes,allowed_resources=None):
    entries={}
    reserved=set()
    allowed_resources=allowed_resources or {}
    def add(name,data):
        validate_release_path(name)
        path=PurePosixPath(name)
        if not name or path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
            raise ValueError('Invalid package path: '+name)
        key=name.casefold()
        if key in reserved:raise ValueError('Duplicate package path: '+name)
        digest=hashlib.sha256(data).hexdigest()
        if digest in dependency_hashes:raise ValueError('External dependency content cannot be bundled: '+name)
        suffix=path.suffix.lower()
        if suffix=='.json' and name!='mod.json':raise ValueError('DMM treats extra JSON documents as patches: '+name)
        if suffix in {'.exe','.dll','.obj','.pdb','.lib','.exp','.pyc','.app_xml'}:
            raise ValueError('Unexpected package binary: '+name)
        if suffix in GAME_RESOURCES and allowed_resources.get(name)!=digest:
            raise ValueError('Unexpected game metadata: '+name)
        if suffix=='.asi' and name!='HexAppeal.asi':raise ValueError('Only the Hex Appeal runtime belongs in this package.')
        if suffix=='.pac' and allowed_pacs.get(name)!=digest:raise ValueError('Unexpected model: '+name)
        reserved.add(key);entries[name]=data
    metadata={'modinfo':{'title':'Hex Appeal','version':version,'author':NEXUS_NAME,'description':SUMMARY}}
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
    if {name for name in entries if PurePosixPath(name).suffix.lower() in GAME_RESOURCES}!=allowed_resources.keys():raise ValueError('Witch part resource output set is incomplete.')
    return [('Hex Appeal/'+name,data) for name,data in sorted(entries.items())]


def player_choices(version,variants,runtime_files,documents,dependency_hashes,interaction_rule):
    if set(variants)!={'Venus','Vanilla'}:
        raise ValueError('The package needs both supported body choices')
    if hashlib.sha256(interaction_rule).hexdigest() in dependency_hashes:
        raise ValueError('External dependency content cannot be bundled: '+WARDROBE_INTERACTION_FILE)
    rule=json.loads(interaction_rule)
    common={}
    output={}
    names={name:('NOTICE-'+name.removeprefix('licenses/') if name.startswith('licenses/') else name.replace('/','--')) for name,_ in documents}
    if len({value.casefold() for value in names.values()})!=len(names):
        raise ValueError('Flattened documentation names must remain unique')
    def document(name,data):
        if name not in names:return name,data
        if name.lower().endswith('.md'):
            def relocate(target):
                if re.match(r'[A-Za-z][A-Za-z0-9+.-]*:|#',target):return target
                path,marker,anchor=target.partition('#')
                resolved=posixpath.normpath(posixpath.join(posixpath.dirname(name),unquote(path)))
                if resolved not in names:return target
                return quote(names[resolved],safe='.-_~')+(marker+anchor if marker else '')
            text=re.sub(r'\[([^\]]*)\]\(([^)]+)\)',lambda match:'['+match.group(1)+']('+relocate(match.group(2))+')',data.decode('utf-8-sig'))
            text=re.sub(r'((?:src|href)\s*=\s*[\"\x27])([^\"\x27]+)([\"\x27])',lambda match:match.group(1)+relocate(match.group(2))+match.group(3),text)
            data=text.encode('utf-8')
        return names[name],data
    for index,mode in enumerate(('Venus','Vanilla'),1):
        variant=variants[mode]
        checked=player_package(version,variant['files'],[],runtime_files,documents,variant['models'],dependency_hashes,variant['resources'])
        choice=f'[{index:02}] '+('VENUS' if mode=='Venus' else 'Vanilla')
        folder='PICK ONE - '+choice
        for name,data in checked:
            name=name.removeprefix('Hex Appeal/')
            if name.startswith('files/'):
                output['Hex Appeal/'+folder+'/'+name.removeprefix('files/')]=data
            elif name!='mod.json':
                name,data=document(name,data)
                if name in common and common[name]!=data:
                    raise ValueError('Body options must share one runtime and documentation set')
                common[name]=data
        detail='Requires the separately installed VENUS Human Body or Elf Body. Includes matching witch outfit refits.' if mode=='Venus' else 'Preserves each witch\'s original body and proportions. VENUS is not required; HEX uses the separately installed preset.'
        metadata={'modinfo':{'title':'PICK ONE: '+choice,'version':version,'author':NEXUS_NAME,'description':detail}}
        output['Hex Appeal/'+folder+'/mod.json']=(json.dumps(metadata,indent=2)+'\n').encode('utf-8')
        output['Hex Appeal/'+folder+'/'+WARDROBE_INTERACTION_FILE]=(
            json.dumps({**rule,**metadata},indent=2)+'\n').encode('utf-8')
    metadata={'modinfo':{'title':'Hex Appeal','version':version,'author':NEXUS_NAME,'description':SUMMARY}}
    common['mod.json']=(json.dumps(metadata,indent=2)+'\n').encode('utf-8')
    if any('/' in name for name in common):raise ValueError('Common files must not become extra DMM choices')
    output.update({'Hex Appeal/'+name:data for name,data in common.items()})
    output['mod.json']=output.pop('Hex Appeal/mod.json')
    return sorted(output.items())
