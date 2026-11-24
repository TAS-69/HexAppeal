from pathlib import Path, PurePosixPath
import copy, hashlib, json, re, xml.etree.ElementTree as ET

from part_registry import read_registry, write_registry
from witch_parts import PRIVATE_BOOTS, isolated_model


BODY_MODELS = {
    'phw_00_nude_00_0001': 'phw_00_nude_00_0001',
    'phw_00_nude_00_0001_damian': 'phw_00_nude_00_0001_damian',
    'phw_00_nude_00_0301': 'phw_00_nude_00_0002',
}
UNDERWEAR_MODELS = {name:'phw_00_uw_00_0002' if name.endswith('_0301') else 'phw_00_uw_00_0001' for name in BODY_MODELS}
UNDERWEAR_TEXTURES = {
    f'character/texture/phw_00_uw_00_0001_{map_name}.dds': f'character/texture/hvw_00_uw_00_0001_{map_name}.dds'
    for map_name in ('ma', 'mg', 'n')
}


def isolate_underwear_material(data):
    for source, target in UNDERWEAR_TEXTURES.items():
        if source.encode() not in data:
            raise ValueError('Original underwear texture binding is missing: ' + source)
        data = data.replace(source.encode(), target.encode())
    ET.fromstring('<Root>' + data.decode('utf-8-sig') + '</Root>')
    return data


def isolate_underwear(prefab,name):
    source='character/model/1_pc/2_phw/armor/38_underwear/'+UNDERWEAR_MODELS[name]+'.pac'
    target=source.replace('/1_pc/','/ha_p/').replace('/phw_','/hvw_')
    if len(source)!=len(target) or prefab.count(source.encode())!=1:
        raise ValueError('The original body underwear reference changed: '+name)
    return prefab.replace(source.encode(),target.encode())


def private_body(name):
    key = name.lower()
    if key not in BODY_MODELS:
        raise ValueError('The original witch body is not supported: ' + name)
    return 'hvw' + key[3:]


def body_registration(registry, name, prefab):
    rows = [row for row in registry['primary'] if row['name'].lower() == name.lower()]
    if len(rows) != 1 or rows[0]['directory'] != '1_pc/02_phw/nude':
        raise ValueError('The original witch body registration changed: ' + name)
    row = copy.deepcopy(rows[0])
    row.update(name=private_body(name), directory='ha_p/02_phw/nude')
    model = BODY_MODELS[name.lower()]
    source = 'character/model/1_pc/2_phw/nude/' + model + '.pac'
    target = source.replace('/1_pc/', '/ha_p/').replace('/phw_', '/hvw_')
    if len(source) != len(target) or prefab.count(source.encode()) != 1:
        raise ValueError('The original body prefab model reference changed: ' + name)
    return row, source, target, prefab.replace(source.encode(), target.encode())


def body_lookup(registry,name):
    rows=[row for row in registry['secondary'] if row[0].lower()==name.lower()]
    if len(rows)!=1 or rows[0][1]!='1_pc/02_phw/nude':
        raise ValueError('The original body lookup registration is missing or changed: '+name)
    return private_body(name),'ha_p/02_phw/nude'


def validate_body_resources(entries):
    registry=read_registry(entries['files/0009/character/bin__/partprefabtable.pappt'])
    primary={row['name']:row for row in registry['primary']}
    secondary=dict(registry['secondary'])
    required=set()
    for name,model in BODY_MODELS.items():
        alias=private_body(name)
        if alias not in primary or primary[alias]['directory']!='ha_p/02_phw/nude' or secondary.get(alias)!='ha_p/02_phw/nude':
            raise ValueError('Original body needs both prefab registrations: '+alias)
        private_model='character/model/ha_p/2_phw/nude/hvw'+model[3:]+'.pac'
        prefab='character/bin__/prefab/ha_p/02_phw/nude/'+alias+'.prefab'
        definition='character/prefab/ha_p/02_phw/nude/'+alias+'.prefabdata_xml'
        resources=(private_model,private_model.replace('character/model/','character/modelproperty/')+'_xml',private_model.replace('character/model/','character/bin__/meshphysics/').removesuffix('.pac')+'.hkx',prefab,definition)
        for path in resources:
            if not entries.get('files/0009/'+path):
                raise ValueError('Original body resource missing: '+path)
        required.update(resources)
        if entries['files/0009/'+prefab].count(private_model.encode())!=1:
            raise ValueError('Original body prefab points at a different model: '+alias)
        root=ET.fromstring(entries['files/0009/'+definition])
        if root.tag!='NudePrefabData' or root.find('BaseCharacterScale') is None or root.find('SkeletonName') is None:
            raise ValueError('Original body definition is incomplete: '+alias)
        underwear='character/model/ha_p/2_phw/armor/38_underwear/hvw'+UNDERWEAR_MODELS[name][3:]+'.pac'
        if entries['files/0009/'+prefab].count(underwear.encode())!=1 or b'character/model/1_pc/2_phw/armor/38_underwear/' in entries['files/0009/'+prefab]:
            raise ValueError('Vanilla underwear must use its private original model: '+alias)
        for path in (underwear,underwear.replace('character/model/','character/modelproperty/')+'_xml',underwear.replace('character/model/','character/bin__/meshphysics/').removesuffix('.pac')+'.hkx'):
            if not entries.get('files/0009/'+path):raise ValueError('Original underwear resource missing: '+path)
            required.add(path)
    return len(required)


def vanilla_patch(data):
    original = ET.fromstring(data)
    body = original.findall('./Nude/Prefab')
    if len(body) != 1:
        raise ValueError('Expected one original witch body')
    name = body[0].get('Name')
    operations = [ET.Element('set', {'find': 'Prefab', 'key': name, 'attr': 'Name', 'value': private_body(name)})]
    for part in original.iter('Prefab'):
        name = part.get('Name')
        if name in PRIVATE_BOOTS:
            operations.append(ET.Element('set', {'find': 'Prefab', 'key': name, 'attr': 'Name', 'value': PRIVATE_BOOTS[name]}))
    after = copy.deepcopy(original)
    for operation in operations:
        matches = [part for part in after.iter('Prefab') if part.get('Name') == operation.get('key')]
        if len(matches) != 1:
            raise ValueError('An original witch part is ambiguous')
        matches[0].set('Name', operation.get('value'))
    for before, changed in zip(original.iter(), after.iter()):
        if before.tag != changed.tag or before.text != changed.text or before.tail != changed.tail:
            raise ValueError('Unrelated original appearance data changed')
        difference = {key for key in before.attrib.keys() | changed.attrib.keys() if before.get(key) != changed.get(key)}
        if difference and (before.tag != 'Prefab' or difference != {'Name'}):
            raise ValueError('Original witch proportions or customization changed')
    patch = '<xml-patch>\n' + ''.join('  ' + ET.tostring(operation, encoding='unicode') + '\n' for operation in operations) + '</xml-patch>\n'
    return patch.encode('utf-8'), body[0].attrib


def build_options(root, sources, native, registry_path, output):
    if output.exists():
        raise FileExistsError('Choose a fresh body-option output directory')
    sha = lambda data: hashlib.sha256(data).hexdigest()
    versions = {'Venus': {}, 'Vanilla': {}}
    for component in ('Hex Appeal - Bodies', 'Hex Appeal - Outfit Refits'):
        base = root / component
        for path in (base / 'files').rglob('*'):
            if path.is_file():
                versions['Venus'][path.relative_to(base).as_posix()] = path.read_bytes()
    for name, data in versions['Venus'].items():
        if PurePosixPath(name).suffix in {'.pappt', '.prefab', '.pac_xml'}:
            versions['Vanilla'][name] = data
    registry = read_registry(registry_path.read_bytes())
    boot_registry = read_registry(versions['Vanilla']['files/0009/character/bin__/partprefabtable.pappt'])
    rows = boot_registry['primary']
    secondary=[]
    provenance = {}
    for name in BODY_MODELS:
        prefab_path = 'character/bin__/prefab/1_pc/02_phw/nude/' + name + '.prefab'
        prefab = (native / '0009' / prefab_path).read_bytes()
        row, original_model, private_model, changed = body_registration(registry, name, prefab)
        changed=isolate_underwear(changed,name)
        rows.append(row)
        secondary.append(body_lookup(registry,name))
        versions['Vanilla']['files/0009/character/bin__/prefab/' + row['directory'] + '/' + row['name'] + '.prefab'] = changed
        definition='character/prefab/1_pc/02_phw/nude/'+name+'.prefabdata_xml'
        definition_data=(native/'0009'/definition).read_bytes()
        definition_root=ET.fromstring(definition_data)
        if definition_root.tag!='NudePrefabData' or definition_root.find('SkeletonName') is None or definition_root.find('BaseCharacterScale') is None:
            raise ValueError('The original body definition is incomplete: '+name)
        definition_target='character/prefab/'+row['directory']+'/'+row['name']+'.prefabdata_xml'
        versions['Vanilla']['files/0009/'+definition_target]=definition_data
        provenance[definition_target]={'source':definition,'sha256':sha(definition_data),'bytes_unchanged':True,'base_character_scale':definition_root.find('BaseCharacterScale').get('Value')}
        for kind, extension in (('model', '.pac'), ('modelproperty', '.pac_xml'), ('bin__/meshphysics', '.hkx')):
            original = original_model.replace('character/model/', 'character/' + kind + '/', 1).removesuffix('.pac') + extension
            target = private_model.replace('character/model/', 'character/' + kind + '/', 1).removesuffix('.pac') + extension
            data = (native / '0009' / original).read_bytes()
            versions['Vanilla']['files/0009/' + target] = data
            provenance[target] = {'source': original, 'sha256': sha(data), 'bytes_unchanged': True}
    for model in sorted(set(UNDERWEAR_MODELS.values())):
        for kind,extension in (('model','.pac'),('modelproperty','.pac_xml'),('bin__/meshphysics','.hkx')):
            original='character/'+kind+'/1_pc/2_phw/armor/38_underwear/'+model+extension
            target=original.replace('/1_pc/','/ha_p/').replace('/phw_','/hvw_')
            data=(native/'0009'/original).read_bytes()
            if extension == '.pac_xml':
                data = isolate_underwear_material(data)
            versions['Vanilla']['files/0009/'+target]=data
            provenance[target]={'source':original,'sha256':sha(data),'bytes_unchanged':extension != '.pac_xml'}
    for source, target in UNDERWEAR_TEXTURES.items():
        data = (native / '0009' / source).read_bytes()
        if not data.startswith(b'DDS '):
            raise ValueError('Expected a decoded stock game texture: ' + source)
        versions['Vanilla']['files/0009/' + target] = data
        provenance[target] = {'source': source, 'sha256': sha(data), 'bytes_unchanged': True}
    assets = json.loads((root / 'build-data/witch-assets.json').read_text(encoding='utf-8'))
    appearance_checks = {}
    for path in sorted({witch['appearance'] for witch in assets}):
        data = (sources / 'vanilla/appearances/0009' / path).read_bytes()
        patch, body = vanilla_patch(data)
        versions['Vanilla']['files/0009/' + path + '.patch'] = patch
        appearance_checks[path] = {'original_body': body, 'private_body': private_body(body['Name']), 'other_attributes_preserved': True}
    refits = json.loads((root / 'build-data/refit-validation.json').read_text(encoding='utf-8'))
    for model in refits['models']:
        private = isolated_model(model['path'])
        if private != model['path']:
            data = (sources / 'vanilla/witch-sources' / model['path']).read_bytes()
            if sha(data) != model['source_sha256']:
                raise ValueError('The original boot source changed')
            versions['Vanilla']['files/' + private] = data
            provenance[private.removeprefix('0009/')] = {'source': model['path'], 'sha256': sha(data), 'bytes_unchanged': True}
    versions['Vanilla']['files/0009/character/bin__/partprefabtable.pappt'] = write_registry({'header': registry['header'], 'primary': rows, 'secondary': secondary})
    verified_body_resources=validate_body_resources(versions['Vanilla'])
    for mode, entries in versions.items():
        for name, data in entries.items():
            target = output / mode / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    report = {'original_appearance_proportions': appearance_checks, 'native_resources': provenance, 'original_body_resources_verified':verified_body_resources,'files': {mode: {name: sha(data) for name, data in entries.items()} for mode, entries in versions.items()}, 'downloaded_dependency_files_bundled': False, 'live_verified': False}
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


if __name__ == '__main__':
    import argparse
    from development_paths import ROOT, SOURCES
    parser = argparse.ArgumentParser(description='Build mutually exclusive VENUS and original-witch body options from external game sources.')
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--registry', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    for path in (args.native, args.registry):
        if path.resolve().is_relative_to(ROOT):
            parser.error('Original game sources must remain outside the project')
    result = build_options(ROOT, SOURCES, args.native, args.registry, args.output)
    print(json.dumps({'files': {mode: len(rows) for mode, rows in result['files'].items()}, 'original_witch_appearances_preserved': len(result['original_appearance_proportions']), 'dependency_files_bundled': False, 'live_verified': False}))
