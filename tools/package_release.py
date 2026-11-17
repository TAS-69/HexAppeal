from pathlib import Path
import argparse,configparser,hashlib,json,re,subprocess,zipfile
from player_package import player_package
from release_policy import validate_release_path,validate_runtime

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Package the requested public version. Does not increment versions, install files or upload.')
parser.add_argument('--draft',action='store_true',help='Allow the recorded blocked preferences test or pending manager acceptance. Keeps release readiness false.')
args=parser.parse_args()
checkout=Path(subprocess.check_output(['git','-C',str(ROOT),'rev-parse','--show-toplevel'],text=True).strip()).resolve()
if checkout!=ROOT:raise SystemExit('Package from a clean Hex Appeal Git checkout.')
if subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain','--untracked-files=all'],text=True).strip():raise SystemExit('Commit the intended release files before packaging; the checkout must be clean.')
tracked=subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode('utf-8').split('\0')
version=(ROOT/'VERSION').read_text(encoding='utf-8-sig').strip()
if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise SystemExit('Invalid public VERSION.')
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
required={'sdk_definition_generator','model','auto_apply','frame_dispatch_abi','menu','input_lifecycle','interface_preferences','creation_chain','wardrobe_instance_ownership','wardrobe_selection','wardrobe_transport_abi','wardrobe_provider_api','underwear_policy','outfit_visibility_ownership','served_dependency_routes','dependency_discovery','native_menu_export','packaged_runtime_configuration','dependency_file_preserved'}
checks={row['name']:row for row in verification['checks']}
required.update({'appearance_hook_compatibility','feature_compatibility'})
if not required<=checks.keys():raise SystemExit('Run the full installed-dependency verification suite.')
failed=[row for row in verification['checks'] if not row['pass']]
draft_exception=args.draft and len(failed)==1 and failed[0]['name']=='interface_preferences' and failed[0].get('blocked')
if failed and not draft_exception:raise SystemExit('Incomplete/failed checks: no release package created. A recorded blocked preferences test may be packaged only with --draft.')
distribution=json.loads((ROOT/'build-data/player-package.json').read_text(encoding='utf-8'))
manager_ready=distribution['manager_acceptance']['verified']
if not manager_ready and not args.draft:raise SystemExit('The combined player package needs its manager import/deployment acceptance check before a public release.')
live=json.loads((ROOT/'build-data/live-visibility-20261117.json').read_text(encoding='utf-8'))
if live['runtime_sha256']!=sha(binary) or not live['temporary_helper_removed_after_game_closed']:raise SystemExit('Record the checked runtime and remove its temporary live test helper first.')
ready=verification['pass'] and manager_ready and not args.draft
settings=configparser.ConfigParser();settings.read(ROOT/'runtime/dist/HexAppeal.ini')
if not all(settings.getboolean('Development',key,fallback=False) for key in ('ObserveNpcUploads','EnableNpcWrites','ObserveWardrobe','EnableWardrobeWrites')):raise SystemExit('Fresh-install features must be enabled.')
output=ROOT/'release'/version
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
allowed_pacs={'Hex Appeal - Outfit Refits/files/'+row['path']:row['output_sha256'] for row in refits['models']}
if len(allowed_pacs)!=40:raise SystemExit('Expected forty default garment refits.')
source_files=[]
for name in sorted(filter(None,tracked)):
    path=ROOT/name
    if not path.is_file():continue
    rel=path.relative_to(ROOT)
    if any(part in private_parts for part in rel.parts):continue
    if path.name=='development-paths.json':continue
    data=path.read_bytes()
    if sha(data) in denied_hashes:raise SystemExit('External dependency file found: '+rel.as_posix())
    if path.suffix.lower() in denied_suffixes:raise SystemExit('Unexpected binary in source: '+rel.as_posix())
    if path.suffix.lower()=='.pac':
        if allowed_pacs.get(rel.as_posix())!=sha(data):raise SystemExit('Unexpected PAC output: '+rel.as_posix())
        continue
    source_files.append((rel.as_posix(),data))
patches=list((ROOT/'Hex Appeal - Bodies/files').rglob('*.patch'))
if len(patches)!=7:raise SystemExit('Expected seven appearance patches.')
summary={'version':version,'release_ready':ready,'draft':not ready,'runtime_sha256':sha(binary),'offline_passed':sum(row['pass'] for row in verification['checks']),'offline_total':len(verification['checks']),'blocked_checks':[{'name':row['name'],'reason':row.get('reason','')} for row in failed],'helper_checks_pass':helpers['pass'],'assets':{'body_patches':7,'vanilla_garment_refits':40,'downloaded_mod_assets_bundled':False,'private_dancer_derivatives_bundled':False},'live_scope':'This binary passed all four independent clothing/Base underwear combinations, repeated restoration, startup restoration, quick pins and unsupported-provider retention on close idle Elowen. Three public SDK game-robe refreshes passed at 500 ms configured delay (625 ms observed). The temporary helper was removed afterward. Earlier core layout import and nine-shape tests retain their original binary; final ZIP not freshly reimported. External colour caches, every witch/detail level/controller and long sessions are not certified. Adopting assets need rendered checks.','licensed_code_adaptations':'Retained with complete notices; separate from external content-mod dependencies.'}
summary['live_verification']=live
summary['distribution']=distribution
summary['distribution_policy']=policy
summary['source_commit']=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
summary_bytes=(json.dumps(summary,indent=2)+'\n').encode()
inventory=[];archives=[]
def archive(label,files):
    path=output/(f'HexAppeal-{label.replace(" ","-")}-{version}.zip' if label else f'HexAppeal-{version}.zip')
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
repo_url='https://github.com/TAS-69/HexAppeal/blob/v'+version+'/'
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
    metadata=json.loads((ROOT/component/'mod.json').read_text(encoding='utf-8-sig'))
    if metadata['modinfo']['version']!=version:raise SystemExit('Component version mismatch.')
    assets.append([(p.relative_to(ROOT/component).as_posix(),p.read_bytes()) for p in (ROOT/component/'files').rglob('*') if p.is_file()])
documents=[('VERIFICATION.txt',summary_bytes),('LICENSE',(ROOT/'LICENSE').read_bytes()),
           ('DEPENDENCIES-AND-REFERENCES.md',package_doc((ROOT/'DEPENDENCIES-AND-REFERENCES.md').read_text(encoding='utf-8-sig'),'').encode())]
for name in ('README.md','INTEGRATION.md'):documents.append(('docs/'+name,package_doc((ROOT/'runtime'/name).read_text(encoding='utf-8-sig'),'runtime').encode()))
documents += [('licenses/'+p.name,p.read_bytes()) for p in (ROOT/'runtime/licenses').iterdir() if p.is_file()]
documents+=guides('docs/')
models={name.removeprefix('Hex Appeal - Outfit Refits/'):digest for name,digest in allowed_pacs.items()}
files=player_package(version,*assets,[('HexAppeal.asi',binary),('HexAppeal.ini',(ROOT/'runtime/dist/HexAppeal.ini').read_bytes())],documents,models,denied_hashes)
archive('',files)
sdk=[(p.relative_to(ROOT/'runtime/sdk').as_posix(),p.read_bytes()) for p in (ROOT/'runtime/sdk').rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc']
sdk.append(('VERIFICATION.json',summary_bytes))
archive('Wardrobe SDK',sdk)
archive('Source',source_files+[('VERIFICATION.json',summary_bytes)])
(output/'VERIFICATION.json').write_bytes(summary_bytes)
(output/'CONTENTS.json').write_text(json.dumps({'version':version,'archives':archives,'files':inventory},indent=2)+'\n',encoding='utf-8')
(output/'SHA256SUMS.txt').write_text(''.join(row['sha256']+'  '+row['file']+'\n' for row in archives),encoding='ascii')
print(json.dumps({'version':version,'release_ready':ready,'archives':archives},indent=2))
