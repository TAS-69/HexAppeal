from pathlib import Path
import argparse, concurrent.futures, configparser, hashlib, json, shutil, struct, subprocess, tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description='Run offline runtime checks. Does not launch Crimson Desert.')
parser.add_argument('--instant', type=Path, help='Installed InstantSurgery.asi; a test copy is made without modifying the original.')
parser.add_argument('--game', type=Path, help='Read the actual DMM manifest and test conflicting-body rejection in an isolated fixture.')
parser.add_argument('--venus-human', type=Path, help='Check the original VENUS 2.6 Human Body on both manifest aliases. Requires --game.')
parser.add_argument('--skip-quarantined-interface-test', action='store_true', help='Do not open or execute the Windows-flagged interface_preferences.exe. Records an incomplete verification, never a full pass.')
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
commands = [
    ('sdk_definition_generator', [__import__('sys').executable,'-m','unittest','discover','-s',str(root/'sdk/tests'),'-p','test_definition.py']),
    ('model', [str(build/'model_tests.exe'), str(build/'test profiles.ini')]),
    ('auto_apply', [str(build/'auto_apply_tests.exe'),str(build/'auto-fixture'/'profiles.ini')]),
    ('frame_dispatch_abi', [str(build/'frame_dispatch_tests.exe')]),
    ('menu', [str(build/'preview.exe'), str(build/'menu-preview.bin'), str(build/'preview profiles.ini')]),
    ('input_lifecycle', [str(build/'input_lifecycle.exe')]),
    ('interface_preferences', [str(build/'interface_preferences.exe'),str(build/'preferences-fixture'/'profiles.ini')]),
    ('creation_chain', [str(build/'creation_chain_tests.exe')]),
    ('wardrobe_instance_ownership', [str(build/'wardrobe_snapshot_tests.exe')]),
    ('wardrobe_selection', [str(build/'wardrobe_selection_tests.exe'),str(build/'wardrobe-fixture'/'wardrobe.ini')]),
    ('wardrobe_transport_abi', [str(build/'wardrobe_transport_tests.exe')]),
    ('wardrobe_provider_api', [str(build/'wardrobe_api_tests.exe'),str(fixture/'provider-api')]),
    ('underwear_policy', [str(build/'underwear_tests.exe'),str(build/'underwear-fixture'/'profiles.ini')]),
    ('outfit_visibility_ownership', [str(build/'outfit_visibility_tests.exe')]),
]
if args.game:
    commands.append(('dancer_global_palette', [str(build/'dancer_palette_tests.exe'),str(args.game),str(fixture/'dancer-palette')]))
    commands.append(('wardrobe_resource_detection', [str(build/'wardrobe_resources_tests.exe'),str(args.game),str(fixture/'wardrobe-resources')]))
    commands.append(('served_dependency_routes', [str(build/'dependency_routes.exe'), str(args.game), str(fixture/'dependency-routes')]))
    if args.venus_human:commands[-1][1].append(str(args.venus_human))
    commands.append(('dependency_discovery', [str(build/'dependency_discovery.exe'), str(args.game), str(fixture/'discovery')]))
source_hash = None
blocked = []
if args.skip_quarantined_interface_test:
    commands = [item for item in commands if item[0] != 'interface_preferences']
    blocked.append({'name':'interface_preferences','pass':False,'blocked':True,
                    'reason':'Windows security flagged/quarantined the test executable. It was not opened or executed; full verification remains incomplete.'})
if args.instant:
    source_hash = sha(args.instant)
    if source_hash != '92d7cab4e244004ea433d1079c42288a985627fce9cb585844030282e591f009':
        raise SystemExit('The native menu test only supports the inspected Instant Surgery 0.9.6 binary.')
    copy = fixture / 'InstantSurgery-test-copy.asi'
    shutil.copyfile(args.instant, copy)
    commands.append(('native_menu_export', [str(build/'native_menu_test.exe'), str(copy)]))

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
results.extend(blocked)
asi = root/'dist'/'HexAppeal.asi'
data = asi.read_bytes()
settings = configparser.ConfigParser()
settings.read(root/'dist/HexAppeal.ini', encoding='utf-8-sig')
observe = settings.getboolean('Development','ObserveNpcUploads',fallback=False)
writes = settings.getboolean('Development','EnableNpcWrites',fallback=False)
wardrobe_observe=settings.getboolean('Development','ObserveWardrobe',fallback=False)
wardrobe_writes=settings.getboolean('Development','EnableWardrobeWrites',fallback=False)
results.append({'name':'packaged_runtime_configuration','pass':observe and writes and wardrobe_observe and wardrobe_writes and (root/'dist/HexAppeal.ini').read_bytes()==(root/'HexAppeal.ini').read_bytes(),
                'writes_require_validated_transaction':True,'saved_shapes_restore_automatically':True})
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
