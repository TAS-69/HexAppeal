from pathlib import Path
import json,hashlib,argparse,sys
from venus_body import active_body,reference_hashes

root=Path(__file__).resolve().parents[1]
def dmm_config_path(game):
    active=game/'nexus_downloads/dmm_update/config.json'
    return active if active.is_file() else game/'config.json'
def check(game):
    required=json.loads((root/'dependencies.json').read_text(encoding="utf-8"))
    checks=[]
    config_path=dmm_config_path(game)
    config=json.loads(config_path.read_text(encoding='utf-8'))
    for dep in required['required']:
        hits=[]
        for pattern in dep['paths']:
            hits.extend(p for p in game.glob(pattern) if p.is_file())
        matching=[p for p in hits if hashlib.sha256(p.read_bytes()).hexdigest() in reference_hashes(dep)]
        passed=bool(matching)
        detail='matching local source present' if passed else 'missing or changed dependency'
        if dep['id']=='venus':
            try:
                variant,body=active_body(config,config_path)
                passed=body.is_file() and hashlib.sha256(body.read_bytes()).hexdigest() in reference_hashes(dep)
                detail='checked '+variant.rsplit('/',1)[-1]+' enabled' if passed else 'The selected VENUS body is missing or differs from the checked references.'
            except ValueError as error:passed=False;detail=str(error)
        checks.append({'id':dep['id'],'name':dep['name'],'status':'PASS' if passed else 'FAIL','detail':detail,'matches':[str(p) for p in matching],'runtime_loaded_verified':False})
    return {'status':'PASS' if all(c['status']=='PASS' for c in checks) else 'FAIL','checks':checks,'dmm_config':str(config_path),'enforcement':'Source availability preflight. Use verify_mount.py and runtime served-dependency guards for mounted assets; this does not prove runtime loading.','runtime_preset_integration':'See docs/wiki/Compatibility.md for tested scope.'}
if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--game',type=Path,required=True)
    args=parser.parse_args()
    result=check(args.game)
    (root/'reports').mkdir(exist_ok=True)
    (root/'reports'/'dependency-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if result['status']=='PASS' else 1)
