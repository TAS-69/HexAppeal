from pathlib import Path
import argparse, hashlib, json

from development_paths import ROOT, SOURCES
from part_registry import read_registry, write_registry
from witch_parts import PRIVATE_BOOTS, clone_boot, partial_registry


def build(registry_path):
    original = registry_path.read_bytes()
    registry = read_registry(original)
    if write_registry(registry) != original:
        raise ValueError('The source part registry did not round trip')
    assets = json.loads((ROOT / 'build-data/witch-assets.json').read_text(encoding='utf-8'))
    output = ROOT / 'Hex Appeal - Outfit Refits/files/0009'
    rows, files, seen = [], {}, set()
    sha = lambda data: hashlib.sha256(data).hexdigest()

    def save(path, data, source):
        target = output / path
        if target.exists() and target.read_bytes() != data:
            raise ValueError('Existing generated resource differs: ' + str(target))
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        files['files/0009/' + path] = {'sha256': sha(data), 'source': source}

    for witch in assets:
        for part in witch['parts']:
            name = part['prefab']
            if name not in PRIVATE_BOOTS or name in seen:
                continue
            seen.add(name)
            if len(part['models']) != 1:
                raise ValueError('The source boot now has multiple models')
            model = part['models'][0]
            source = SOURCES / 'vanilla/witch-sources/0009'
            prefab = (source / part['prefab_path']).read_bytes()
            row, private_model, private_prefab = clone_boot(registry, name, prefab, model)
            rows.append(row)
            model_property = model.replace('character/model/', 'character/modelproperty/', 1) + '_xml'
            private_property = private_model.replace('character/model/', 'character/modelproperty/', 1) + '_xml'
            property_data = (source / model_property).read_bytes()
            save(private_property, property_data, {'kind': 'original game model metadata', 'path': model_property, 'sha256': sha(property_data)})
            path = 'character/bin__/prefab/' + row['directory'] + '/' + row['name'] + '.prefab'
            save(path, private_prefab, {'kind': 'original game prefab with one model path changed', 'path': part['prefab_path'], 'sha256': sha(prefab)})
    save('character/bin__/partprefabtable.pappt', partial_registry(registry, rows), {'kind': 'three additive game part registrations', 'primary_source_sha256': sha(original)})
    report = {'format': 1, 'private_boot_parts': PRIVATE_BOOTS, 'resources': files, 'downloaded_dependency_files_bundled': False}
    (ROOT / 'build-data/witch-part-resources.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build witch-only boot metadata from separately extracted game files.')
    parser.add_argument('--registry', type=Path, required=True, help='Original game partprefabtable.pappt, kept outside the project')
    args = parser.parse_args()
    if args.registry.resolve().is_relative_to(ROOT):
        parser.error('Keep original game sources outside the project')
    result = build(args.registry)
    print('Built', len(result['resources']), 'checked game metadata resources. No dependency files copied.')
