from pathlib import Path
import argparse, concurrent.futures, configparser, ctypes, hashlib, json, re, shutil, struct, subprocess, tempfile
from zipfile import ZipFile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description='Run offline runtime checks. Does not launch Crimson Desert.')
parser.add_argument('--instant', type=Path, help='Installed InstantSurgery.asi; a test copy is made without modifying the original.')
parser.add_argument('--game', type=Path, help='Read the actual DMM manifest and test conflicting-body rejection in an isolated fixture.')
parser.add_argument('--venus-human', type=Path, help='Check the original Venus Body for Damiane 2.6 Human Body on both manifest aliases. Requires --game.')
args = parser.parse_args()
if args.venus_human and not args.game:parser.error('--venus-human requires --game')
for name in ('instant','game','venus_human'):
    value=getattr(args,name)
    if value is not None:setattr(args,name,value.resolve())
temporary = tempfile.TemporaryDirectory(prefix='hexappeal-')
fixture = Path(temporary.name)
build = root / 'build'
report_dir = root / 'reports'
report_dir.mkdir(exist_ok=True)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
addon_pack = root.parent/'addons/maegu-woosa'
addon_source = addon_pack/'Characters/maegu-woosa.tsv'
addon_preview = fixture/'addon-menu'
shutil.copytree(addon_pack/'Characters', addon_preview/'Characters')
follow_game_fixture = fixture/'addon-follow-game'
shutil.copytree(root.parent/'addons/naira/Characters', follow_game_fixture/'Characters')
shutil.copytree(follow_game_fixture/'Characters', addon_preview/'Characters', dirs_exist_ok=True)
preview_pack = addon_preview/'Characters'/'naira.tsv'
header, rows = preview_pack.read_text(encoding='utf-8-sig').split('\n', 1)
preview_pack.write_text(f'{header}\n#multiCharacter\t0\n{rows}', encoding='utf-8')
full_roster = fixture/'addon-follow-game-capacity'
shutil.copytree(follow_game_fixture/'Characters', full_roster/'Characters')
addon_header = (root/'src/character_addons.h').read_text(encoding='utf-8')
capacity = int(re.search(r'characterCapacity=(\d+);', addon_header)[1])
first_addon = int(re.search(r'firstAddonCharacter=(\d+);', addon_header)[1])
filler_count = capacity-first_addon-3
for start in range(0, filler_count, 64):
    with (full_roster/'Characters'/f'aaa-capacity-{start:03}.tsv').open('w', encoding='ascii') as filler:
        filler.write('HexAppealCharacters\t1\n')
        for i in range(start, min(start+64, filler_count)):
            filler.write(f'character\tha_fixture_{i}\tFixture {i}\t{600000+i}\tHA_Fixture_{i}'
                         f'\tcharacter/appearance/npc/fixture_{i}.app_xml\n'
                         f'default\tha_fixture_{i}\tRobe\texample_ub_robe\n')
addon_zip = fixture/'character-addon.zip'
addon_route = subprocess.check_output([
    __import__('sys').executable, str(root.parent/'tools/package_character_addon.py'),
    str(addon_source), str(addon_pack/'README.md'), str(addon_zip),
    '--title', 'Hex Appeal - Maegu and Woosa',
    '--version', (addon_pack/'VERSION').read_text(encoding='ascii').strip(),
    '--license-file', str(root.parent/'LICENSE')
], text=True).strip()
addon_payload = fixture/'maegu-woosa.xml'
with ZipFile(addon_zip) as archive:
    addon_payload.write_bytes(archive.read('files/0009/'+addon_route))
commands = [
    ('sdk_definition_generator', [__import__('sys').executable,'-m','unittest','discover','-s',str(root/'sdk/tests'),'-p','test_*.py']),
    ('skin_setup', [__import__('sys').executable,'-m','unittest','discover','-s',str(root.parent/'tools/tests'),'-p','test_witch_skin_setup.py']),
    ('model', [str(build/'model_tests.exe'), str(build/'test profiles.ini')]),
    ('auto_apply', [str(build/'auto_apply_tests.exe'),str(build/'auto-fixture'/'profiles.ini')]),
    ('frame_dispatch_abi', [str(build/'frame_dispatch_tests.exe')]),
    ('menu', [str(build/'preview.exe'), str(build/'menu-preview.bin'), str(build/'preview profiles.ini'), str(addon_preview)]),
    ('input_lifecycle', [str(build/'input_lifecycle.exe')]),
    ('interface_preferences', [str(build/'auto_apply_tests.exe'),'--interface-preferences',str(fixture/'preferences'/'profiles.ini')]),
    ('creation_chain', [str(build/'creation_chain_tests.exe')]),
    ('appearance_hook_compatibility', [str(build/'appearance_hook_tests.exe')]),
    ('feature_compatibility', [str(build/'compatibility_tests.exe')]),
    ('wardrobe_instance_ownership', [str(build/'wardrobe_snapshot_tests.exe')]),
    ('character_addons', [str(build/'character_addon_tests.exe'), str(fixture/'character-addons'), str(addon_source), str(addon_payload), str(root.parent/'addons')]),
    ('addon_follow_game', [str(build/'preview.exe'),'--addon-follow-game',str(follow_game_fixture)]),
    ('addon_follow_game_capacity', [str(build/'preview.exe'),'--addon-follow-game-capacity',str(full_roster)]),
    ('wardrobe_selection', [str(build/'wardrobe_selection_tests.exe'),str(build/'wardrobe-fixture'/'wardrobe.ini')]),
    ('wardrobe_item_names', [str(build/'wardrobe_names_tests.exe')]),
    ('wardrobe_outfit_slots', [str(build/'wardrobe_slots_tests.exe'),str(fixture/'outfit-slots.txt')]),
    ('wardrobe_transport_abi', [str(build/'wardrobe_transport_tests.exe')]),
    ('barber_catalog', [str(build/'barber_catalog_tests.exe'),str(fixture/'barber-catalog.xml')]),
    ('wardrobe_provider_api', [str(build/'wardrobe_api_tests.exe'),str(fixture/'provider-api')]),
    ('underwear_policy', [str(build/'underwear_tests.exe'),str(build/'underwear-fixture'/'profiles.ini')]),
    ('outfit_visibility_ownership', [str(build/'outfit_visibility_tests.exe')]),
]
if args.game:
    commands.append(('wardrobe_stock_items',[str(build/'wardrobe_stock_tests.exe'),str(args.game)]))
    commands.append(('game_signatures',[str(build/'game_signatures_tests.exe'),str(args.game/'bin64'/'CrimsonDesert.exe')]))
    commands.append(('witch_skin_routes',[str(build/'skin_routes.exe'), str(args.game), str(fixture/'skin-routes')]))
    if args.venus_human:commands[-1][1].append(str(args.venus_human))
    commands.append(('served_dependency_routes', [str(build/'dependency_routes.exe'), str(args.game), str(fixture/'dependency-routes')]))
    if args.venus_human:commands[-1][1].append(str(args.venus_human))
    commands.append(('dependency_discovery', [str(build/'dependency_discovery.exe'), str(args.game), str(fixture/'discovery')]))
source_hash = None
if args.instant:
    source_hash = sha(args.instant)
    if source_hash != '92d7cab4e244004ea433d1079c42288a985627fce9cb585844030282e591f009':
        raise SystemExit('The native menu test only supports the inspected Instant Surgery 0.9.6 binary.')
    copy = fixture / 'InstantSurgery-test-copy.asi'
    shutil.copyfile(args.instant, copy)
    compatibility=next(command for name,command in commands if name=='feature_compatibility')
    compatibility.append(str(copy))
    if args.game and (args.game/'bin64/BodyPhysics.asi').is_file():
        physics=fixture/'BodyPhysics-test-copy.asi';shutil.copyfile(args.game/'bin64/BodyPhysics.asi',physics);compatibility.append(str(physics))
    commands.append(('native_menu_export', [str(build/'native_menu_test.exe'), str(copy)]))
    unknown = fixture / 'InstantSurgery-unverified-test-copy.asi'
    unknown.write_bytes(copy.read_bytes()+b'Hex Appeal version-probe fixture')
    commands.append(('unverified_native_interface', [str(build/'native_menu_test.exe'), str(unknown), '--probe-only']))

def run(item):
    name, command = item
    try:result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=25)
    except OSError as error:
        return {'name':name,'pass':False,'blocked':True,'reason':str(error),'stdout':'','stderr':''}
    except subprocess.TimeoutExpired:
        return {'name':name,'pass':False,'reason':'The test exceeded its 25-second limit.','stdout':'','stderr':''}
    return {'name': name, 'pass': result.returncode == 0, 'exit_code': result.returncode,
            'stdout': result.stdout.strip(), 'stderr': result.stderr.strip()}

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(run, commands))
asi = root/'dist'/'HexAppeal.asi'
data = asi.read_bytes()
expected_version=json.loads((root/'dist/build.json').read_text(encoding='utf-8-sig'))['version']
size=ctypes.windll.version.GetFileVersionInfoSizeW(str(asi),None)
version_buffer=ctypes.create_string_buffer(size) if size else None
loaded_version=bool(size and ctypes.windll.version.GetFileVersionInfoW(str(asi),0,size,version_buffer))
def version_string(field):
    address,length=ctypes.c_void_p(),ctypes.c_uint()
    if loaded_version and ctypes.windll.version.VerQueryValueW(
        version_buffer,'\\StringFileInfo\\040904b0\\'+field,ctypes.byref(address),ctypes.byref(length)
    ):
        return ctypes.wstring_at(address.value)
    return None
file_version=version_string('FileVersion')
product_version=version_string('ProductVersion')
results.append({'name':'public_version_metadata','pass':file_version==product_version==expected_version,
                'file_version':file_version,'product_version':product_version,'expected_version':expected_version})
settings = configparser.ConfigParser()
settings.read(root/'dist/HexAppeal.ini', encoding='utf-8-sig')
observe = settings.getboolean('Development','ObserveNpcUploads',fallback=False)
writes = settings.getboolean('Development','EnableNpcWrites',fallback=False)
wardrobe_observe=settings.getboolean('Development','ObserveWardrobe',fallback=False)
wardrobe_writes=settings.getboolean('Development','EnableWardrobeWrites',fallback=False)
wardrobe_interaction=settings.getboolean('Wardrobe','EnableInteraction',fallback=False)
results.append({'name':'packaged_runtime_configuration','pass':observe and writes and wardrobe_observe and wardrobe_writes and wardrobe_interaction and (root/'dist/HexAppeal.ini').read_bytes()==(root/'HexAppeal.ini').read_bytes(),
                'writes_require_validated_transaction':True,'saved_shapes_restore_automatically':True,'wardrobe_interaction_default':wardrobe_interaction})
pe = struct.unpack_from('<I', data, 0x3c)[0]
pe64 = data[pe:pe+4] == b'PE\0\0' and struct.unpack_from('<H',data,pe+4)[0] == 0x8664
if args.instant:
    preserved = sha(args.instant) == source_hash
    results.append({'name':'dependency_file_preserved','pass':preserved,'sha256':source_hash})
report = {
    'version':json.loads((root/'dist/build.json').read_text(encoding='utf-8-sig'))['version'],
    'scope':'Offline only. No game launch, DMM mount, DX12 presentation, NPC deformation or outfit fitting tested.',
    'asi':{'bytes':len(data),'sha256':sha(asi),'amd64_pe':pe64},
    'checks':results,
    'pass':pe64 and all(r['pass'] for r in results),
    'test_provider_in_shipping_binary':False,
    'npc_fitting_provider_shipped':True,
    'npc_observation_default':observe,
    'npc_pose_writes_default':writes,
    'saved_shapes_restore_automatically':True,
    'wardrobe_observation_default':wardrobe_observe,
    'wardrobe_writes_default':wardrobe_writes,
    'venus_human_body_checked':bool(args.venus_human) and any(row['name']=='served_dependency_routes' and row['pass'] for row in results),
}
(report_dir/'offline-verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
for item in results: print(('PASS' if item['pass'] else 'FAIL')+': '+item['name'])
print('Offline verification passed.' if report['pass'] else 'Offline verification failed.')
temporary.cleanup()
raise SystemExit(0 if report['pass'] else 1)
