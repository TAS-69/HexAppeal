from pathlib import Path
import sys,json,hashlib,xml.etree.ElementTree as ET,argparse

root=Path(__file__).resolve().parents[1]
(root/'reports').mkdir(exist_ok=True)
from development_paths import SOURCES,dependency,use_toolchain
from venus_body import active_body,refit_reference
use_toolchain()
from core.mesh_parser import parse_mesh
from core.compression_engine import _decompress_type1_par
from pac_lods import lod_layout

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
parser=argparse.ArgumentParser()
parser.add_argument('--game',type=Path)
args=parser.parse_args()
project=json.loads((root/'project.json').read_text(encoding="utf-8"))
report=json.loads((root/'build-data'/'refit-validation.json').read_text(encoding="utf-8"))
manifest=json.loads((root/'build-data'/'witch-assets.json').read_text(encoding="utf-8"))
venus=dependency('venus',args.game)
metadata=next(item for item in json.loads((root/'dependencies.json').read_text(encoding='utf-8'))['required'] if item['id']=='venus')
baseline=refit_reference(metadata,project['required_body']['pac_sha256'])
assert refit_reference(metadata,sha(venus))==baseline
assert refit_reference(metadata,report['method']['venus_sha256'])==baseline
assert not list((root/'Hex Appeal - Bodies').rglob('*.pac'))
checks=[]
for path in sorted({t['appearance'] for t in manifest}):
    source=SOURCES/'vanilla'/'appearances'/'0009'/path
    patch=root/'Hex Appeal - Bodies'/'files'/'0009'/(path+'.patch')
    before=ET.fromstring(source.read_bytes())
    after=ET.fromstring(source.read_bytes())
    operations=ET.fromstring(patch.read_bytes())
    assert operations.tag=='xml-patch' and len(operations)==1
    op=operations[0]
    original=before.find('./Nude/Prefab').get('Name')
    assert op.tag=='set' and op.attrib=={'find':'Prefab','key':original,'attr':'Name','value':'phw_00_nude_00_0001_damian'}
    matches=[p for p in after.iter(op.attrib['find']) if p.get('Name')==op.attrib['key']]
    assert len(matches)==1
    matches[0].set(op.attrib['attr'],op.attrib['value'])
    before.find('./Nude/Prefab').set('Name','phw_00_nude_00_0001_damian')
    assert ET.tostring(before)==ET.tostring(after)
    checks.append({'path':path,'status':'pass'})
for item in report['models']:
    assert 'error' not in item,item
    assert item['triangle_normal_flips']==0,item
    assert item['skin_uv_indices_preserved'] and item['unrelated_bytes_preserved']
    source=SOURCES/'vanilla'/'witch-sources'/item['path']
    output=root/'Hex Appeal - Outfit Refits'/'files'/item['path']
    assert sha(source)==item['source_sha256'],source
    assert sha(output)==item['output_sha256'],output
    data=output.read_bytes()
    mesh=parse_mesh(data,output.name)
    assert mesh.total_vertices==item['lod0_vertices'] and mesh.total_faces==item['lod0_faces']
    d0,l0=lod_layout(_decompress_type1_par(source.read_bytes()))
    d1,l1=lod_layout(data)
    assert l0==l1
    assert [d.name for d in d0]==[d.name for d in d1]
    assert [d.material for d in d0]==[d.material for d in d1]
    checks.append({'path':item['path'],'status':'pass'})
assert len(report['models'])==40
runtime={'checked':False}
if args.game:
    from check_dependencies import check,dmm_config_path
    dependencies=check(args.game)
    (root/'reports'/'dependency-validation.json').write_text(json.dumps(dependencies,indent=2)+'\n')
    assert dependencies['status']=='PASS','One or more required local dependency files are missing or changed.'
    config=json.loads(dmm_config_path(args.game).read_text(encoding='utf-8'))
    selected,current=active_body(config,dmm_config_path(args.game))
    assert refit_reference(metadata,sha(current))==baseline,'VENUS changed: review the outfit add-on before using it.'
    runtime={'checked':True,'enabled_variant':selected,'pac_sha256':sha(current),'shared_refit_reference_sha256':baseline}
out={'scope':'Offline asset and source dependency checks only; see dmm-mount-verification.json and live-acceptance.json for separate live evidence.','offline_checks':len(checks),'status':'PASS','checks':checks,'venus_dependency':runtime,'in_game_tested':False,'dmm_mounted':False}
(root/'reports'/'validation-summary.json').write_text(json.dumps(out,indent=2)+'\n')
print('PASS:',len(checks),'offline asset checks; VENUS dependency', 'verified' if args.game else 'not checked against DMM')
print('This command does not launch or mount the game. Separate mount and live acceptance reports are retained.')
