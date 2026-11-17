from pathlib import Path
import hashlib,json,subprocess,sys

root=Path(__file__).resolve().parents[1]
result=subprocess.run([sys.executable,'-m','unittest','discover','-s',str(root/'tools/tests')],cwd=root,capture_output=True,text=True)
inputs=[root/'tools/prepare_dancer_dependencies.py',root/'tools/Setup Dancer.cmd',root/'tools/shading_frames.py',root/'tools/venus_body.py',root/'tools/player_package.py',root/'tools/package_release.py',root/'build-data/player-package.json',root/'dependencies.json',*sorted((root/'tools/tests').glob('*.py'))]
report={'pass':result.returncode==0,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr,'inputs':{p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
(root/'runtime/reports').mkdir(parents=True,exist_ok=True)
(root/'runtime/reports/helper-verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(result.stdout+result.stderr)
raise SystemExit(result.returncode)
