from pathlib import Path
import argparse,configparser,hashlib,json,re,subprocess,tempfile,zipfile
from player_package import GAME_RESOURCES, WARDROBE_INTERACTION_FILE, player_choices
from release_policy import REQUIRED_RUNTIME_CHECKS,manager_acceptance_matches_runtime,validate_release_path,validate_runtime
from witch_parts import isolated_model
from body_options import build_options
from development_paths import SOURCES
from package_character_addon import character_addon_entries
from release_version import check_version_metadata, release_version

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Package the requested public version. Does not increment versions, install files or upload.')
mode_parser=parser.add_mutually_exclusive_group()
mode_parser.add_argument('--draft',action='store_true',help='Allow the recorded blocked preferences test or pending manager acceptance. Keeps release readiness false.')
mode_parser.add_argument('--candidate',action='store_true',help='Build the release-shaped archives for manual DMM import and live acceptance. All offline tests must pass.')
parser.add_argument('--native',type=Path,required=True,help='External original game body, underwear and prefab inputs.')
parser.add_argument('--registry',type=Path,required=True,help='External original partprefabtable.pappt.')
parser.add_argument('--native-report',type=Path,help='Isolated native shaping verification for the exact binary.')
parser.add_argument('--output',type=Path,help='A new output directory outside the source checkout.')
args=parser.parse_args()
checkout=Path(subprocess.check_output(['git','-C',str(ROOT),'rev-parse','--show-toplevel'],text=True).strip()).resolve()
if checkout!=ROOT:raise SystemExit('Package from a clean Hex Appeal Git checkout.')
if subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain','--untracked-files=all'],text=True).strip():raise SystemExit('Commit the intended release files before packaging; the checkout must be clean.')
tracked=subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode('utf-8').split('\0')
version=release_version(ROOT)
check_version_metadata(ROOT,version)
sha=lambda data:hashlib.sha256(data).hexdigest()
asi=ROOT/'runtime/dist/HexAppeal.asi'
binary=asi.read_bytes()
validate_runtime(binary)
build=json.loads((ROOT/'runtime/dist/build.json').read_text(encoding='utf-8-sig'))
verification=json.loads((ROOT/'runtime/reports/offline-verification.json').read_text(encoding='utf-8-sig'))
helpers=json.loads((ROOT/'runtime/reports/helper-verification.json').read_text(encoding='utf-8-sig'))
if not helpers['pass'] or any(sha((ROOT/name).read_bytes())!=digest for name,digest in helpers['inputs'].items()):raise SystemExit('Verify the current helper sources first.')
if build['version']!=version or not build['public_release'] or build['sha256']!=sha(binary):raise SystemExit('Build this public version first.')
if verification['version']!=version or verification['asi']['sha256']!=sha(binary):raise SystemExit('Verify this exact binary first.')
checks={row['name']:row for row in verification['checks']}
missing=sorted(REQUIRED_RUNTIME_CHECKS-checks.keys())
if missing:raise SystemExit('Missing installed-dependency verification: '+', '.join(missing))
runtime_version=checks['public_version_metadata']
if any(runtime_version.get(field)!=version for field in ('file_version','product_version')):
    raise SystemExit('The compiled ASI FileVersion and ProductVersion must match the public mod version.')
failed=[row for row in verification['checks'] if not row['pass']]
draft_exception=args.draft and len(failed)==1 and failed[0]['name']=='interface_preferences' and failed[0].get('blocked')
if (failed or not verification['pass']) and not draft_exception:raise SystemExit('Incomplete/failed checks: no release package created. A recorded blocked preferences test may be packaged only with --draft.')
distribution=json.loads((ROOT/'build-data/player-package.json').read_text(encoding='utf-8'))
manager=distribution['manager_acceptance']
manager_report=json.loads((ROOT/manager['import_report']).read_text(encoding='utf-8')) if manager.get('import_report') else {}
manager_ready=manager_acceptance_matches_runtime(manager,manager_report,sha(binary))
if not manager_ready and not (args.draft or args.candidate):raise SystemExit('The final combined ZIP with this runtime needs a fresh DMM import and deployment check.')
live=json.loads((ROOT/'build-data/live-compatibility-20261118.json').read_text(encoding='utf-8'))
if not args.candidate:
    if live['public_runtime_sha256']!=sha(binary) or not live['public_startup_checked'] or not live['temporary_helpers_absent']:raise SystemExit('Record the public runtime startup check and remove temporary live helpers first.')
    if not all(live['body_options'][mode]['passed'] for mode in ('Venus','Vanilla')):raise SystemExit('Both body options need their recorded live acceptance.')
native=json.loads((args.native_report or ROOT/'build-data/native-compatibility-20261118.json').read_text(encoding='utf-8'))
if not native['pass'] or native['runtime_sha256']!=sha(binary):raise SystemExit('Record the current isolated native shaping verification first.')
ready=verification['pass'] and manager_ready and not (args.draft or args.candidate)
settings=configparser.ConfigParser();settings.read(ROOT/'runtime/dist/HexAppeal.ini')
if not all(settings.getboolean('Development',key,fallback=False) for key in ('ObserveNpcUploads','EnableNpcWrites','ObserveWardrobe','EnableWardrobeWrites')):raise SystemExit('Fresh-install features must be enabled.')
if not settings.getboolean('Wardrobe','EnableInteraction',fallback=False):raise SystemExit('Fresh installs must enable the Wardrobe interaction.')
output=args.output.resolve() if args.output else ROOT/'release'/version
if args.output and output.is_relative_to(ROOT):raise SystemExit('Choose an output directory outside the checkout.')
output.mkdir(parents=True,exist_ok=True)
if list(output.iterdir()):raise SystemExit('Output already exists. Preserve it and choose a fresh requested version or move the previous local output aside.')
denied_suffixes={'.asi','.dll','.exe','.obj','.pdb','.lib','.exp','.pyc','.pab','.dds','.bin','.hkx','.pac_xml','.app_xml'}
private_parts={'.git','__pycache__','build','dist','reports','release','permission','backups','research','sources','toolchain','development-assets'}
dependency=json.loads((ROOT/'dependencies.json').read_text(encoding='utf-8-sig'))
denied_hashes={item['sha256'] for item in dependency['required']}
denied_hashes.update(body['sha256'] for item in dependency['required'] for body in item.get('verified_variants',[]))
policy=json.loads((ROOT/'build-data/distribution-policy.json').read_text(encoding='utf-8'))
denied_hashes.update(policy['excluded_dependency_sha256'])
denied_hashes.update(item['sha256'] for item in dependency.get('optional',[]) if 'sha256' in item)
refits=json.loads((ROOT/'build-data/refit-validation.json').read_text(encoding='utf-8-sig'))
allowed_pacs={'Hex Appeal - Outfit Refits/files/'+isolated_model(row['path']):row['output_sha256'] for row in refits['models']}
if len(allowed_pacs)!=40:raise SystemExit('Expected forty default garment refits.')
resources=json.loads((ROOT/'build-data/witch-part-resources.json').read_text(encoding='utf-8'))
allowed_resources={'Hex Appeal - Outfit Refits/'+name:row['sha256'] for name,row in resources['resources'].items()}
source_files=[]
for name in sorted(filter(None,tracked)):
    path=ROOT/name
    if not path.is_file():continue
    rel=path.relative_to(ROOT)
    if any(part in private_parts for part in rel.parts):continue
    if path.name=='development-paths.json':continue
    data=path.read_bytes()
    if sha(data) in denied_hashes:raise SystemExit('External dependency file found: '+rel.as_posix())
    if rel.as_posix() in allowed_resources:
        if allowed_resources[rel.as_posix()]!=sha(data):raise SystemExit('Generated game metadata changed: '+rel.as_posix())
        committed=subprocess.check_output(['git','-C',str(ROOT),'show','HEAD:'+rel.as_posix()])
        if committed!=data:raise SystemExit('Preserve original game metadata bytes in Git: '+rel.as_posix())
        continue
    if path.suffix.lower() in denied_suffixes:raise SystemExit('Unexpected binary in source: '+rel.as_posix())
    if path.suffix.lower()=='.pac':
        if allowed_pacs.get(rel.as_posix())!=sha(data):raise SystemExit('Unexpected PAC output: '+rel.as_posix())
        continue
    source_files.append((rel.as_posix(),data))
patches=list((ROOT/'Hex Appeal - Bodies/files').rglob('*.patch'))
if len(patches)!=7:raise SystemExit('Expected seven appearance patches.')
summary={'version':version,'release_ready':ready,'draft':not ready,'runtime_sha256':sha(binary),'offline_passed':sum(row['pass'] for row in verification['checks']),'offline_total':len(verification['checks']),'blocked_checks':[{'name':row['name'],'reason':row.get('reason','')} for row in failed],'helper_checks_pass':helpers['pass'],'assets':{'body_patches_per_option':7,'venus_garment_refits':40,'downloaded_mod_assets_bundled':False,'private_dancer_derivatives_bundled':False},'live_scope':live['scope'],'licensed_code_adaptations':'Retained with complete notices; separate from external content-mod dependencies.'}
if args.candidate:
    summary['live_scope']='Manual DMM ZIP import, mount and in-game acceptance are pending for this archive.'
    summary['live_verification']={'public_startup_checked':False,'manual_zip_import_checked':False,
        'platform_passes':{'vanilla_without_venus':False,'vanilla_with_damiane_venus':False,'hex_appeal_venus':False},
        'asset_baseline_version':live['public_version']}
    distribution['manager_acceptance']={**distribution['manager_acceptance'],'verified':False,'final_zip_freshly_imported':False,
        'acceptance_scope':'The exact combined ZIP needs manual DMM import, mount and in-game verification in all three body setups.'}
else:
    summary['live_verification']=live
summary['native_verification']=native
summary['distribution']=distribution
summary['distribution_policy']=policy
summary['source_commit']=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
summary_bytes=(json.dumps(summary,indent=2)+'\n').encode()
inventory=[];archives=[]
def archive(label,files,archive_version=version):
    path=output/(f'HexAppeal-{label.replace(" ","-")}-{archive_version}.zip' if label else f'HexAppeal-{archive_version}.zip')
    names=set()
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as dest:
        for name,data in sorted(files):
            validate_release_path(name)
            if name in names or '..' in Path(name).parts or name.startswith('/'):raise SystemExit('Invalid archive path.')
            names.add(name)
            info=zipfile.ZipInfo(name,date_time=(2026,11,16,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
            dest.writestr(info,data)
            inventory.append({'archive':path.name,'file':name,'sha256':sha(data),'bytes':len(data)})
    with zipfile.ZipFile(path) as src:
        if src.testzip():raise SystemExit('ZIP integrity failure.')
    archives.append({'file':path.name,'bytes':path.stat().st_size,'sha256':sha(path.read_bytes())})
repo_url='https://github.com/TAS-69/HexAppeal/blob/main/'
def package_doc(text,origin):
    def link(match):
        label,target=match.group(1),match.group(2)
        if re.match(r'[a-z]+:|#',target):return match.group(0)
        local=(ROOT/origin/target.split('#')[0]).resolve()
        if local.is_relative_to(ROOT) and local.exists():
            if local.parent==ROOT/'docs/wiki' and origin=='docs/wiki':return match.group(0)
            return '['+label+']('+repo_url+local.relative_to(ROOT).as_posix().replace(' ','%20')+')'
        return match.group(0)
    return re.sub(r'(?<!!)\[([^\]]+)\]\(([^)]+)\)',link,text)
def guides(prefix):
    files=[]
    for path in sorted((ROOT/'docs/wiki').glob('*.md')):
        if path.name.startswith('_'):continue
        text=path.read_text(encoding='utf-8-sig')
        text=package_doc(text,'docs/wiki')
        files.append((prefix+'Player Guide/'+path.name,text.encode()))
    for path in sorted((ROOT/'docs/assets').iterdir()):files.append((prefix+'assets/'+path.name,path.read_bytes()))
    return files
assets=[]
for component in ('Hex Appeal - Bodies','Hex Appeal - Outfit Refits'):
    assets.append([(p.relative_to(ROOT/component).as_posix(),p.read_bytes()) for p in (ROOT/component/'files').rglob('*') if p.is_file()])
documents=[('VERIFICATION.txt',summary_bytes),('LICENSE',(ROOT/'LICENSE').read_bytes()),
           ('DEPENDENCIES-AND-REFERENCES.md',package_doc((ROOT/'DEPENDENCIES-AND-REFERENCES.md').read_text(encoding='utf-8-sig'),'').encode())]
for name in ('README.md','INTEGRATION.md'):documents.append(('docs/'+name,package_doc((ROOT/'runtime'/name).read_text(encoding='utf-8-sig'),'runtime').encode()))
documents += [('licenses/'+p.name,p.read_bytes()) for p in (ROOT/'runtime/licenses').iterdir() if p.is_file()]
documents+=guides('docs/')
models={name.removeprefix('Hex Appeal - Outfit Refits/'):digest for name,digest in allowed_pacs.items()}
metadata={name.removeprefix('Hex Appeal - Outfit Refits/'):digest for name,digest in allowed_resources.items()}
for original in (args.native,args.registry):
    if original.resolve().is_relative_to(ROOT):raise SystemExit('Keep original game inputs outside the source checkout.')
with tempfile.TemporaryDirectory(prefix='body-options-',dir=ROOT/'runtime/build') as temporary:
    temporary=Path(temporary).resolve()
    if not temporary.is_relative_to((ROOT/'runtime/build').resolve()):raise SystemExit('Invalid temporary output path.')
    report=build_options(ROOT,SOURCES,args.native,args.registry,temporary/'options')
    variants={}
    for mode,expected in report['files'].items():
        entries=[(name,(temporary/'options'/mode/name).read_bytes()) for name in expected]
        if any(sha(data)!=expected[name] for name,data in entries):raise SystemExit('Body option output changed during packaging.')
        variants[mode]={'files':entries,'models':{name:digest for name,digest in expected.items() if name.endswith('.pac')},'resources':{name:digest for name,digest in expected.items() if Path(name).suffix in GAME_RESOURCES}}
    summary['assets']['body_option_files']={mode:len(entries) for mode,entries in report['files'].items()}
    summary['assets']['original_body_resources_verified']=report['original_body_resources_verified']
    expected_assets=live['body_option_asset_sha256']
    if report['files']!=expected_assets:raise SystemExit('Packaged body options differ from the live-tested resources.')
    summary_bytes=(json.dumps(summary,indent=2)+'\n').encode()
    documents=[(name,summary_bytes if name=='VERIFICATION.txt' else data) for name,data in documents]
    files=player_choices(version,variants,[('HexAppeal.asi',binary),('HexAppeal.ini',(ROOT/'runtime/dist/HexAppeal.ini').read_bytes())],documents,denied_hashes,(ROOT/'build-data'/WARDROBE_INTERACTION_FILE).read_bytes())
archive('',files)
addon=ROOT/'addons/maegu-woosa'
addon_version=(addon/'VERSION').read_text(encoding='ascii').strip()
_, addon_files=character_addon_entries(addon/'Characters/maegu-woosa.tsv',addon/'README.md',
                                      'Hex Appeal - Maegu and Woosa',addon_version,
                                      license_file=ROOT/'LICENSE')
archive('Maegu and Woosa Add-on',addon_files.items(),addon_version)
sdk=[(p.relative_to(ROOT/'runtime/sdk').as_posix(),p.read_bytes()) for p in (ROOT/'runtime/sdk').rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc']
sdk.append(('VERIFICATION.json',summary_bytes))
archive('Wardrobe SDK',sdk)
archive('Source',source_files+[('VERIFICATION.json',summary_bytes)])
(output/'VERIFICATION.json').write_bytes(summary_bytes)
(output/'CONTENTS.json').write_text(json.dumps({'version':version,'archives':archives,'files':inventory},indent=2)+'\n',encoding='utf-8')
(output/'SHA256SUMS.txt').write_text(''.join(row['sha256']+'  '+row['file']+'\n' for row in archives),encoding='ascii')
print(json.dumps({'version':version,'release_ready':ready,'archives':archives},indent=2))
