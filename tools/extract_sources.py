from pathlib import Path
import argparse,sys,json,hashlib,importlib.util,os

root=Path(__file__).resolve().parents[1]
from development_paths import SOURCES,TOOLCHAIN,dependency,use_toolchain
use_toolchain()
spec=importlib.util.spec_from_file_location('hex_archive_reader',TOOLCHAIN/'cdumm'/'paz_parse.py')
reader=importlib.util.module_from_spec(spec)
sys.modules[spec.name]=reader
spec.loader.exec_module(reader)
from core.crypto_engine import decrypt
from core.compression_engine import decompress

parser=argparse.ArgumentParser(description='Read and verify the known Hex Appeal source paths. Does not write to the game.')
parser.add_argument('--game',type=Path,required=True)
args=parser.parse_args()
snapshots=json.loads((root/'build-data'/'source-snapshots.json').read_text(encoding="utf-8"))
for identifier in ('venus','hex'):
    dependency(identifier,args.game)
indexes={}
staged=[]
for item in snapshots:
    dest=(SOURCES/Path(item['snapshot']).relative_to('sources')).resolve()
    assert dest.is_relative_to(SOURCES.resolve())
    if 'installed_path' in item:
        continue
    else:
        group=item['group']
        if group not in indexes:
            indexes[group]={e.path:e for e in reader.parse_pamt(str(args.game/group/'0.pamt'),full_paths=True)}
        e=indexes[group][item['virtual_path']]
        size=e.comp_size if e.compressed else e.orig_size
        with open(e.paz_file,'rb') as f:
            f.seek(e.offset);data=f.read(size)
        assert len(data)==size
        ext=Path(e.path).suffix.lower()
        if ext in ['.xml','.paloc','.css','.html','.pami','.uianiminit','.spline2d','.spline','.mi','.txt','.app_xml','.pac_xml','.prefabdata_xml']:
            data=decrypt(data,os.path.basename(e.path))
        if e.compressed and e.compression_type:
            data=decompress(data,e.orig_size,e.compression_type)
    assert hashlib.sha256(data).hexdigest()==item['sha256'],f'Source changed: {item["snapshot"]}. A fresh asset audit is required.'
    staged.append((dest,data))
for dest,data in staged:
    dest.parent.mkdir(exist_ok=True,parents=True)
    dest.write_bytes(data)
print('Verified external mod dependencies; extracted',len(staged),'vanilla inputs to',SOURCES,'. Game files were read only.')
