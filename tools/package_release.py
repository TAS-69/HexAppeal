from pathlib import Path
import argparse,configparser,hashlib,json,re,subprocess,zipfile

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Package the requested public version. Does not increment versions, install files or upload.')
parser.add_argument('--draft',action='store_true',help='Allow the specifically recorded blocked interface-preferences test. Keeps release readiness false.')
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
build=json.loads((ROOT/'runtime/dist/build.json').read_text(encoding='utf-8-sig'))
verification=json.loads((ROOT/'runtime/reports/offline-verification.json').read_text(encoding='utf-8-sig'))
helpers=json.loads((ROOT/'runtime/reports/helper-verification.json').read_text(encoding='utf-8-sig'))
if not helpers['pass'] or any(sha((ROOT/name).read_bytes())!=digest for name,digest in helpers['inputs'].items()):raise SystemExit('Verify the current helper sources first.')
if build['version']!=version or not build['public_release'] or build['sha256']!=sha(binary):raise SystemExit('Build this public version first.')
if verification['version']!=version or verification['asi']['sha256']!=sha(binary):raise SystemExit('Verify this exact binary first.')
required={'sdk_definition_generator','model','auto_apply','frame_dispatch_abi','menu','input_lifecycle','interface_preferences','creation_chain','wardrobe_instance_ownership','wardrobe_selection','wardrobe_transport_abi','wardrobe_provider_api','underwear_policy','outfit_visibility_ownership','dancer_global_palette','wardrobe_resource_detection','served_dependency_routes','dependency_discovery','native_menu_export','packaged_runtime_configuration','dependency_file_preserved'}
checks={row['name']:row for row in verification['checks']}
if not required<=checks.keys():raise SystemExit('Run the full installed-dependency verification suite.')
failed=[row for row in verification['checks'] if not row['pass']]
draft_exception=args.draft and len(failed)==1 and failed[0]['name']=='interface_preferences' and failed[0].get('blocked')
if failed and not draft_exception:raise SystemExit('Incomplete/failed checks: no release package created. A recorded blocked preferences test may be packaged only with --draft.')
ready=verification['pass'] and not args.draft
settings=configparser.ConfigParser();settings.read(ROOT/'runtime/dist/HexAppeal.ini')
if not all(settings.getboolean('Development',key,fallback=False) for key in ('ObserveNpcUploads','EnableNpcWrites','ObserveWardrobe','EnableWardrobeWrites')):raise SystemExit('Fresh-install features must be enabled.')
output=ROOT/'release'/version
output.mkdir(parents=True,exist_ok=True)
if list(output.iterdir()):raise SystemExit('Output already exists. Preserve it and choose a fresh requested version or move the previous local output aside.')
denied_suffixes={'.asi','.dll','.exe','.obj','.pdb','.lib','.exp','.pyc','.pab','.dds','.bin'}
private_parts={'.git','__pycache__','build','dist','reports','release','permission','backups','research','sources','toolchain','development-assets'}
dependency=json.loads((ROOT/'dependencies.json').read_text(encoding='utf-8-sig'))
denied_hashes={item['sha256'] for item in dependency['required']}
denied_hashes.add(dependency['optional'][0]['source_body_sha256'])
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
summary={'version':version,'release_ready':ready,'draft':not ready,'runtime_sha256':sha(binary),'offline_passed':sum(row['pass'] for row in verification['checks']),'offline_total':len(verification['checks']),'blocked_checks':[{'name':row['name'],'reason':row.get('reason','')} for row in failed],'helper_checks_pass':helpers['pass'],'assets':{'body_patches':7,'vanilla_garment_refits':40,'downloaded_mod_assets_bundled':False,'private_dancer_derivatives_bundled':False},'live_scope':'Previous clean development binary: sampled Elowen idle startup restoration, nine Dancer shape combinations, White/Black automatic refresh and Default restoration. This release rebuild has version-string/configuration changes and has not had a fresh live session.','licensed_code_adaptations':'Retained with complete notices; separate from external content-mod dependencies.'}
summary_bytes=(json.dumps(summary,indent=2)+'\n').encode()
inventory=[];archives=[]
def archive(label,files):
    path=output/f'Hex Appeal - {label}-{version}.zip'
    names=set()
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as dest:
        for name,data in sorted(files):
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
for component,label in (('Hex Appeal - Bodies','Bodies'),('Hex Appeal - Outfit Refits','Outfit Refits')):
    files=[(p.relative_to(ROOT).as_posix(),p.read_bytes()) for p in (ROOT/component).rglob('*') if p.is_file()]
    metadata=json.loads((ROOT/component/'mod.json').read_text(encoding='utf-8-sig'))
    if metadata['modinfo']['version']!=version:raise SystemExit('Component version mismatch.')
    files +=[(component+'/LICENSE', (ROOT/'LICENSE').read_bytes()),(component+'/DEPENDENCIES-AND-REFERENCES.md',package_doc((ROOT/'DEPENDENCIES-AND-REFERENCES.md').read_text(encoding='utf-8-sig'),'').encode()),(component+'/VERIFICATION.json',summary_bytes)]
    archive(label,files)
files=[('bin64/HexAppeal.asi',binary),('bin64/HexAppeal.ini',(ROOT/'runtime/dist/HexAppeal.ini').read_bytes()),('bin64/HexAppeal/VERIFICATION.json',summary_bytes)]
for name in ('README.md','INTEGRATION.md','LICENSE'):files.append(('bin64/HexAppeal/'+name,package_doc((ROOT/'runtime'/name).read_text(encoding='utf-8-sig'),'runtime').encode()))
files.append(('bin64/HexAppeal/DEPENDENCIES-AND-REFERENCES.md',package_doc((ROOT/'DEPENDENCIES-AND-REFERENCES.md').read_text(encoding='utf-8-sig'),'').encode()))
files += [('bin64/HexAppeal/licenses/'+p.name,p.read_bytes()) for p in (ROOT/'runtime/licenses').iterdir() if p.is_file()]
files+=guides('bin64/HexAppeal/')
archive('Runtime',files)
sdk=[(p.relative_to(ROOT/'runtime/sdk').as_posix(),p.read_bytes()) for p in (ROOT/'runtime/sdk').rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc']
sdk.append(('VERIFICATION.json',summary_bytes))
archive('Wardrobe SDK',sdk)
archive('Source',source_files+[('VERIFICATION.json',summary_bytes)])
(output/'VERIFICATION.json').write_bytes(summary_bytes)
(output/'CONTENTS.json').write_text(json.dumps({'version':version,'archives':archives,'files':inventory},indent=2)+'\n',encoding='utf-8')
(output/'SHA256SUMS.txt').write_text(''.join(row['sha256']+'  '+row['file']+'\n' for row in archives),encoding='ascii')
print(json.dumps({'version':version,'release_ready':ready,'archives':archives},indent=2))
