"""External inputs for local development; game-mod source files are never staged here."""
from pathlib import Path
import hashlib, json, os, sys
from venus_body import reference_hashes

ROOT=Path(__file__).resolve().parents[1]
settings=ROOT/'development-paths.json'
CONFIG=json.loads(settings.read_text(encoding='utf-8-sig')) if settings.is_file() else {}
GAME=Path(os.environ.get('HEX_APPEAL_GAME',CONFIG.get('game',r'C:\Program Files (x86)\Steam\steamapps\common\Crimson Desert'))).resolve()
default=ROOT.parent/'hexappeal-inputs'
RESEARCH=Path(os.environ.get('HEX_APPEAL_RESEARCH',CONFIG.get('researchRoot',str(default)))).resolve()
if RESEARCH.is_relative_to(ROOT):raise ValueError('Development inputs must be outside the Hex Appeal project.')
SOURCES=RESEARCH/'sources'
TOOLCHAIN=RESEARCH/'toolchain'

def use_toolchain():
    sys.path[:0]=[str(TOOLCHAIN/'python-deps'),str(TOOLCHAIN/'crimsonforge'),str(ROOT/'tools')]

def dependency(identifier,game=None):
    game=Path(game or GAME).resolve()
    meta=next(item for item in json.loads((ROOT/'dependencies.json').read_text(encoding='utf-8'))['required'] if item['id']==identifier)
    candidates=[]
    override=CONFIG.get('dependencyFiles',{}).get(identifier)
    if override:candidates=[Path(override)]
    else:
        for pattern in meta['paths']:candidates.extend(game.glob(pattern))
        snapshots=json.loads((ROOT/'build-data/source-snapshots.json').read_text(encoding='utf-8'))
        candidates.extend(game/item['installed_path'] for item in snapshots if item.get('installed_path') and item['sha256']==meta['sha256'])
        manifest=game/'bin64/dmm/manifest.tsv'
        if identifier=='venus' and manifest.is_file():
            for line in manifest.read_text(encoding='utf-8-sig').splitlines():
                parts=line.split('\t')
                if len(parts)==3 and parts[0]=='file' and parts[1].replace('\\','/').lower()=='character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac':
                    target=Path(parts[2]);candidates.insert(0,target if target.is_absolute() else manifest.parent/target)
    for path in candidates:
        path=path.resolve()
        if not path.is_relative_to(ROOT) and path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() in reference_hashes(meta):return path
    raise FileNotFoundError(f'External {meta["name"]} {meta["version"]} not found. Set dependencyFiles.{identifier} in development-paths.json.')
