"""Plan external DMM links to unmodified, separately obtained Dancer resources."""
from pathlib import Path
import argparse
import hashlib
import json
import os

PROJECT = Path(__file__).resolve().parents[1]
PLAYER_MARKER = 'HEX-APPEAL-PACKAGE.txt'
PLAYER_MARKER_TEXT = 'HexAppeal.PlayerPackage/1\n'
COLOURS = {
    'white': ('00 Color - Original White', '1547547f672fc339019269fe52714273439f7ae7a63f90a14a2194be83ff1056'),
    'black': ('10 Color - Midnight Black', 'a6afb66e561a2bb0877cdce7166d4a71dc665e7de284deeea0e6e1f66120a237'),
    'crimson': ('20 Color - Crimson Red', '9c5d65142c2cd1d8748cadfd01f42e6fc314a80c18e440495403afcc982dbe2b'),
    'pink': ('30 Color - Arcane Pink', '740de77a37b2f4d28cecb8de53c74a0783f799ed27502fa2fd42fd4585dcfef0'),
    'emerald': ('40 Color - Emerald Green', 'e6dc9ce64307a60d0ece5ee1d10e33f84d6cc83303a5fe871d19238f737bdfd7')
}
UPPER = 'character/model/1_pc/2_phw/armor/9_upperbody/cd_phw_00_ub_00_0191.pac'
LOWER = 'character/model/1_pc/2_phw/armor/10_lowerbody/cd_phw_00_lb_00_0191.pac'
PROPERTY = 'character/modelproperty/1_pc/2_phw/armor/9_upperbody/cd_phw_00_ub_00_0191.pac_xml'
TEXTURE = 'character/texture/cd_phw_00_ub_00_0135_00_02_01_o.dds'
KNOWN = {UPPER: 'a81ba8af6d42774c2d39bf0efd12845920633268be9c246065bb91043f2139ee',
         LOWER: '11cac17e563dc5e6a9efbfb26072fedb238e8b2ebffd88395d5f13ec6f7eb2e5',
         PROPERTY: '9914560195a89e5cb885691f1284a69449d2d51e7c4954dce8d55627874124d6'}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def ordinary(path):
    for candidate in (path, *path.parents):
        if candidate.exists() and (candidate.is_symlink() or candidate.is_junction()):
            raise ValueError(f'Choose an ordinary folder without redirected paths: {candidate}')

def readable(path, virtual):
    ordinary(path)
    if not path.is_file():
        raise ValueError(f'Missing dependency file: {path}')
    with path.open('rb') as stream:
        header = stream.read(16)
    signature = b'PAR ' if virtual.endswith('.pac') else b'DDS ' if virtual.endswith('.dds') else None
    if not header or signature is not None and not header.startswith(signature):
        raise ValueError(f'Unreadable or incorrect file format: {path}')
    return sha(path)

def installed_package(destination):
    ordinary(destination)
    marker=destination/PLAYER_MARKER
    if not marker.is_file() or marker.read_text(encoding='ascii')!=PLAYER_MARKER_TEXT:
        raise ValueError('Choose the installed combined Hex Appeal package, with its package marker.')
    metadata=json.loads((destination/'mod.json').read_text(encoding='utf-8-sig'))
    if metadata.get('modinfo',{}).get('title')!='Hex Appeal' or metadata['modinfo'].get('author')!='TAS-69':
        raise ValueError('The destination is not the combined Hex Appeal package.')
    if not all((destination/name).is_file() for name in ('HexAppeal.asi','HexAppeal.ini')):
        raise ValueError('The installed package runtime or configuration is missing.')

def detect_source(main_mod):
    installed_package(main_mod)
    candidates=[]
    for candidate in main_mod.parent.iterdir():
        if not candidate.is_dir() or candidate==main_mod:continue
        textures=[candidate/folder/TEXTURE for folder,_ in COLOURS.values()]
        if not all(path.is_file() for path in textures):continue
        try:
            checked=[readable(candidate/folder/TEXTURE,TEXTURE)==known for folder,known in COLOURS.values()]
            for virtual in (UPPER,LOWER,PROPERTY):checked.append(readable(candidate/COLOURS['white'][0]/virtual,virtual)==KNOWN[virtual])
        except ValueError:continue
        candidates.append((candidate,all(checked)))
    verified=[path for path,known in candidates if known]
    if len(verified)==1:return verified[0]
    if len(candidates)==1:return candidates[0][0]
    if candidates:raise ValueError('Several Dancer downloads were found. Choose the original FOMOD with --source.')
    raise ValueError('Dancer source not found. Keep the complete five-colour FOMOD in the same mods folder, or choose it with --source.')

def plan(source, destination, selected='white', integrated=False):
    ordinary(source); ordinary(destination)
    source, destination = source.resolve(), destination.resolve()
    if selected not in COLOURS:
        raise ValueError('Choose one of the supplied colours.')
    if source.is_relative_to(PROJECT) or (destination.is_relative_to(PROJECT) and not integrated):
        raise ValueError('Keep the dependency and its local bindings outside Hex Appeal.')
    if source.is_relative_to(destination) or destination.is_relative_to(source):
        raise ValueError('Source and binding folders must be separate.')
    if integrated:
        installed_package(destination)
        generated=[destination/'files/0009'/virtual for virtual in (UPPER,LOWER,PROPERTY,TEXTURE)]
        generated += [destination/name for name in ('HEX-APPEAL-DANCER-BINDINGS.txt','EXTERNAL-SOURCES.txt','DANCER-SETUP.txt')]
        if any(path.exists() for path in generated):raise ValueError('Dancer references already exist; no installed files will be overwritten.')
    elif destination.exists():
        raise ValueError('Choose a new binding folder; existing mods are never overwritten.')
    warnings = []
    palette = []
    for name, (folder, known) in COLOURS.items():
        path = source / folder / TEXTURE
        digest = readable(path, TEXTURE)
        palette.append({'colour': name, 'path': str(path), 'sha256': digest})
        if digest != known:
            warnings.append(f'{folder} has not been verified. Functionality is not guaranteed.')
    rows = []
    for virtual in [UPPER, LOWER, PROPERTY, TEXTURE]:
        path = source / COLOURS[selected][0] / virtual
        digest = readable(path, virtual)
        known = COLOURS[selected][1] if virtual == TEXTURE else KNOWN[virtual]
        if digest != known and virtual != TEXTURE:
            warnings.append(f'{virtual} has not been verified. Functionality is not guaranteed.')
        if path.drive.casefold() != destination.drive.casefold():
            raise ValueError('Hard links need the dependency and binding folder on the same drive.')
        rows.append({'virtual_path': virtual, 'link_source': str(path), 'sha256': digest,
                     'target': str(destination / 'files/0009' / virtual)})
    return {'version': 2, 'destination': str(destination), 'source_mod': str(source), 'integrated_player_package': integrated,
            'source_nexus_mod': 2891, 'source_author': 'Vincent1681 / VincentQuirin',
            'binding_author': 'TAS-69', 'model_source': 'unmodified external dependency',
            'initial_global_colour': selected, 'warnings': warnings, 'verified_reference': not warnings,
            'files': rows, 'palette': palette, 'bindings_created': False,
            'scope': 'Four local references to original external files. No mesh editing, prefab patch, game launch or DMM mount.',
            'redistributable': False}

def apply(report):
    destination = Path(report['destination'])
    ordinary(destination)
    integrated=report.get('integrated_player_package',False)
    if integrated:
        installed_package(destination)
        if any((destination/'files/0009'/virtual).exists() for virtual in (UPPER,LOWER,PROPERTY,TEXTURE)):
            raise ValueError('A Dancer target appeared after planning; no files changed.')
    elif destination.exists():
        raise ValueError('Binding folder appeared after planning; no files changed.')
    expected = {str(destination / 'files/0009' / virtual) for virtual in [UPPER, LOWER, PROPERTY, TEXTURE]}
    if len(report['files']) != 4 or {row['target'] for row in report['files']} != expected:
        raise ValueError('The binding plan does not contain exactly the four expected routes.')
    for row in report['files'] + report['palette']:
        path = Path(row.get('link_source', row.get('path')))
        ordinary(path)
        if sha(path) != row['sha256']:
            raise ValueError('A dependency changed after planning; no files changed.')

    destination.mkdir(parents=True,exist_ok=integrated)
    created=[]
    try:
        for row in report['files']:
            target, source = Path(row['target']), Path(row['link_source'])
            if not target.resolve().is_relative_to(destination.resolve()):
                raise ValueError('Invalid binding target.')
            target.parent.mkdir(parents=True, exist_ok=True)
            os.link(source, target);created.append(target)
            if not os.path.samefile(source, target) or sha(target) != row['sha256']:
                raise RuntimeError('External binding verification failed.')
    except Exception:
        for target in reversed(created):target.unlink()
        raise
    metadata = {'modinfo': {'title': 'Hex Appeal - External Dancer Resources (local)',
                           'author': 'TAS-69', 'version': (PROJECT/'VERSION').read_text(encoding='utf-8-sig').strip() if (PROJECT/'VERSION').is_file() else '1.0.0',
                           'description': 'Local links to separately installed Sexy Witches of Pywel Reshaped by Vincent1681 / VincentQuirin. Unmodified source models and supplied colours. No body/default-witch replacement. Do not redistribute.'}}
    if not integrated:(destination / 'mod.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    (destination / 'HEX-APPEAL-DANCER-BINDINGS.txt').write_text('HexAppeal.ExternalDancerColourBinding/1\n', encoding='ascii', newline='\n')
    report['bindings_created'] = True
    (destination / 'EXTERNAL-SOURCES.txt').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    note='LOCAL DEPENDENCY REFERENCES — DO NOT REDISTRIBUTE\n\nKeep the separately obtained Sexy Witches FOMOD. These four links expose its original upper/lower models, properties and chosen supplied texture. No source file is edited. Hard links share data: never edit linked file contents. Colour changes replace only the generated link.\n\nRefresh DMM and mount with the game closed. Review shared 0191 conflicts. Keep VENUS active. This tool does not mount or test anything in game.\n'
    if integrated:note+='\nThese references belong to the installed Hex Appeal entry. No separate Dancer resource entry is needed. Disable the previous external resource entry before mounting. The public ZIP does not contain these dependency files.\n'
    (destination / ('DANCER-SETUP.txt' if integrated else 'README.txt')).write_text(note,encoding='utf-8')
    return report

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, help='Separate extracted FOMOD root containing all five colour folders. Main-package mode can detect it beside the installed mod.')
    targets=parser.add_mutually_exclusive_group(required=True)
    targets.add_argument('--destination', type=Path, help='New external DMM binding folder on the same drive.')
    targets.add_argument('--main-mod', type=Path, help='Existing installed combined Hex Appeal folder. Adds local references without a separate mod entry.')
    parser.add_argument('--colour', choices=COLOURS, default='white')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--apply', action='store_true', help='Create the planned links. Omit to inspect only.')
    args = parser.parse_args()
    destination=args.main_mod if args.main_mod is not None else args.destination
    if args.source is None and args.main_mod is None:parser.error('--source is required with --destination')
    source=args.source if args.source is not None else detect_source(args.main_mod.resolve())
    report = plan(source,destination,args.colour,integrated=args.main_mod is not None)
    if args.report:
        ordinary(args.report)
        if args.report.exists() or args.report.resolve().is_relative_to(source.resolve()):
            raise ValueError('Choose a fresh report path outside the source dependency.')
        if args.report.resolve().is_relative_to(destination.resolve()):
            raise ValueError('Keep the report outside the DMM binding folder.')
    if args.apply:
        apply(report)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
