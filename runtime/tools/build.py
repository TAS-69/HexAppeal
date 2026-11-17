from pathlib import Path
import subprocess,os,json,concurrent.futures,hashlib,shutil

root=Path(__file__).resolve().parents[1]
import argparse,re
parser=argparse.ArgumentParser(description='Build Hex Appeal and its offline checks. Version files are never incremented.')
parser.add_argument('--release',action='store_true',help='Use the public VERSION. Default: DEV_VERSION with a development suffix.')
args=parser.parse_args()
version=(root.parent/('VERSION' if args.release else 'DEV_VERSION')).read_text(encoding='utf-8-sig').strip()
if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise SystemExit('Expected a three-part version.')
build_version=version if args.release else version+'-development'

build=root/'build';build.mkdir(exist_ok=True)
(build/'release_version.h').write_text('#pragma once\n#define HEX_APPEAL_BUILD_VERSION '+json.dumps(build_version)+'\n',encoding='utf-8')
out=root/'dist';out.mkdir(exist_ok=True)
locator=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
if not locator.is_file():raise SystemExit('Install Visual Studio 2022 C++ x64 tools and Windows SDK.')
installation=subprocess.check_output([str(locator),'-latest','-version','[17.0,18.0)','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
vcvars=Path(installation)/'VC/Auxiliary/Build/vcvars64.bat'
if not vcvars.is_file():raise SystemExit('MSVC x64 tools were not found.')
env_script=build/'msvc-env.cmd'
env_script.write_text('@echo off\ncall "'+str(vcvars)+'" >nul\nif errorlevel 1 exit /b 1\nset\n',encoding='utf-8')
result=subprocess.run(['cmd.exe','/d','/c',str(env_script)],capture_output=True,text=True,check=True)
env=os.environ.copy()
for line in result.stdout.splitlines():
    if '=' in line:
        key,value=line.split('=',1)
        if key:env[key]=value
compiler=shutil.which('cl.exe',path=env.get('Path',env.get('PATH')))
linker=shutil.which('link.exe',path=env.get('Path',env.get('PATH')))
if not compiler or not linker:raise SystemExit('MSVC x64 compiler or linker was not found.')
imgui=root/'vendor'/'imgui'
includes=[root/'src',root/'include',root/'sdk',root/'build',imgui,imgui/'backends',root/'vendor'/'minhook'/'include',root/'vendor'/'minhook'/'src'/'hde']
flags=['/nologo','/c','/O2','/MT','/DNDEBUG','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_CRT_SECURE_NO_WARNINGS','/DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD','/EHsc','/utf-8']
incs=['/I'+str(i) for i in includes]
def job(source,tag='',definitions=()):
    name=source.relative_to(root).as_posix().replace('/','_')+tag+'.obj'
    dest=build/name
    args=[compiler,*flags,*incs,*['/D'+d for d in definitions]]
    if source.suffix=='.cpp':args+=['/std:c++17']
    else:args+=['/TC']
    args+=['/Fo'+str(dest),str(source)]
    run=subprocess.run(args,env=env,capture_output=True,text=True)
    return source,dest,run
imgui_sources=[imgui/n for n in ['imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp','backends/imgui_impl_win32.cpp']]
sources=list((root/'src').rglob('*.cpp'))+imgui_sources+list((root/'vendor'/'minhook'/'src').glob('*.c'))+[root/'vendor'/'minhook'/'src'/'hde'/'hde64.c']
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results=list(pool.map(job,sources))
log=[]
for src,dest,run in results:
    log.append(str(src)+'\n'+run.stdout+run.stderr)
(build/'compile.log').write_text('\n'.join(log),encoding='utf-8')
failed=[(src,run) for src,dest,run in results if run.returncode]
if failed:
    for src,run in failed:print(str(src)+'\n'+run.stdout+run.stderr)
    raise SystemExit(1)
objects={src.relative_to(root).as_posix():dest for src,dest,run in results}
assembler=shutil.which('ml64.exe',path=env.get('Path',env.get('PATH')))
if not assembler:raise SystemExit('MSVC x64 assembler was not found.')
frame_object=build/'frame_dispatch.asm.obj'
subprocess.run([assembler,'/nologo','/c','/Fo'+str(frame_object),str(root/'src/hooks/frame_dispatch.asm')],env=env,check=True,capture_output=True)
objects['src/hooks/frame_dispatch.asm']=frame_object
libs=['d3d12.lib','dxgi.lib','dwmapi.lib','imm32.lib','dxguid.lib','user32.lib','gdi32.lib','psapi.lib','bcrypt.lib','shell32.lib']
def link(name,objs,dll=False):
    command=[linker,'/nologo','/INCREMENTAL:NO','/OUT:'+str(name),*['/DLL']*dll,*map(str,objs),*libs]
    run=subprocess.run(command,env=env,capture_output=True,text=True)
    if run.returncode:print(run.stdout+run.stderr);raise SystemExit(run.returncode)
link(out/'HexAppeal.asi',objects.values(),True)
shutil.copy2(root/'HexAppeal.ini',out/'HexAppeal.ini')
(out/'build.json').write_text(json.dumps({'version':build_version,'public_release':args.release,'sha256':hashlib.sha256((out/'HexAppeal.asi').read_bytes()).hexdigest()},indent=2)+'\n',encoding='utf-8')
def extra(source,tag='',definitions=()):
    src,obj,run=job(source,tag,definitions)
    if run.returncode:print(run.stdout+run.stderr);raise SystemExit(run.returncode)
    return obj
model=objects['src/model.cpp'];runtime=objects['src/runtime.cpp']
dependency_objects=[objects['src/mounted_dependencies.cpp'],objects['src/dependency_paths.cpp']]
link(build/'model_tests.exe',[model,extra(root/'tests'/'model_tests.cpp')])
link(build/'frame_dispatch_tests.exe',[frame_object,extra(root/'tests'/'frame_dispatch_tests.cpp')])
link(build/'auto_apply_tests.exe',[model,runtime,*dependency_objects,extra(root/'tests'/'auto_apply_tests.cpp')])
link(build/'native_menu_test.exe',[model,runtime,*dependency_objects,extra(root/'tests'/'native_menu_test.cpp')])
link(build/'dependency_routes.exe',[model,runtime,*dependency_objects,extra(root/'tests'/'dependency_routes.cpp')])
link(build/'dependency_discovery.exe',[model,runtime,*dependency_objects,extra(root/'tests'/'dependency_discovery.cpp')])
link(build/'wardrobe_resources_tests.exe',[model,runtime,*dependency_objects,objects['src/wardrobe_resources.cpp'],objects['src/dancer_palette.cpp'],extra(root/'tests/wardrobe_resources.cpp')])
link(build/'dancer_palette_tests.exe',[model,runtime,*dependency_objects,objects['src/wardrobe_resources.cpp'],objects['src/dancer_palette.cpp'],extra(root/'tests/dancer_palette.cpp')])
link(build/'wardrobe_api_tests.exe',[objects['src/wardrobe_api.cpp'],extra(root/'tests/wardrobe_api.cpp')])
link(build/'outfit_visibility_tests.exe',[objects['src/outfit_visibility.cpp'],extra(root/'tests/outfit_visibility.cpp')])
extra(root/'tests/wardrobe_c_abi.c')
extra(root/'sdk/examples/WardrobeProvider.cpp')
subprocess.run([__import__('sys').executable,str(root/'sdk/tools/generate_provider.py'),str(root/'sdk/examples/outfit.json'),'--output',str(build/'ProviderDefinition.hpp'),'--force'],check=True,capture_output=True)
extra(root/'sdk/examples/NativeOutfitProvider.cpp')
subprocess.run([__import__('sys').executable,str(root/'sdk/tools/generate_provider.py'),str(root/'sdk/examples/parts-outfit.json'),'--output',str(build/'PartsDefinition.hpp'),'--force'],check=True,capture_output=True)
extra(root/'sdk/examples/PartsOutfitProvider.cpp')
minhook=[obj for src,obj in objects.items() if src.startswith('vendor/minhook/')]
link(build/'underwear_tests.exe',[model,runtime,*dependency_objects,objects['src/pose_access.cpp'],objects['src/outfit_visibility.cpp'],extra(root/'src/underwear.cpp','.visibility',['HEX_UNDERWEAR_TESTS']),extra(root/'tests/underwear_tests.cpp','',['HEX_UNDERWEAR_TESTS']),*minhook])
preview_objs=[objects['vendor/imgui/'+n] for n in ['imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp']]
preview_objs += [model,runtime,*dependency_objects,extra(root/'src'/'menu.cpp','.preview',['HEX_MENU_TESTS']),extra(root/'tests'/'preview.cpp','',['HEX_MENU_TESTS'])]
preview_objs += [objects['src/outfit_visibility.cpp'],objects['src/underwear.cpp'],objects['src/pose_access.cpp'],objects['src/wardrobe.cpp'],objects['src/wardrobe_snapshot.cpp'],objects['src/wardrobe_transport.cpp'],objects['src/wardrobe_selection.cpp'],objects['src/wardrobe_native.cpp'],objects['src/wardrobe_resources.cpp'],objects['src/dancer_palette.cpp'],objects['src/wardrobe_api.cpp'],objects['src/hooks/frame_dispatch.cpp'],frame_object]
preview_objs += [obj for src,obj in objects.items() if src.startswith('vendor/minhook/')]
link(build/'preview.exe',preview_objs)
input_objs=[objects['vendor/imgui/'+n] for n in ['imgui.cpp','imgui_draw.cpp','imgui_widgets.cpp','imgui_tables.cpp','backends/imgui_impl_win32.cpp']]
input_objs+=[obj for src,obj in objects.items() if src.startswith('vendor/minhook/')]
input_objs+=[model,runtime,*dependency_objects,extra(root/'src'/'hooks'/'input.cpp','.test',['MODACTIVATOR_TESTS']),extra(root/'tests'/'input_lifecycle.cpp')]
input_objs+=[extra(root/'src'/'hooks'/'xinput_hook.cpp','.test',['MODACTIVATOR_TESTS'])]
link(build/'input_lifecycle.exe',input_objs)
link(build/'interface_preferences.exe',[model,runtime,*dependency_objects,extra(root/'tests'/'interface_preferences.cpp')])
link(build/'creation_chain_tests.exe',[extra(root/'tests'/'creation_chain.cpp')])
link(build/'appearance_hook_tests.exe',[extra(root/'tests'/'appearance_hooks.cpp')])
link(build/'compatibility_tests.exe',[model,runtime,*dependency_objects,extra(root/'tests'/'compatibility.cpp')])
link(build/'wardrobe_snapshot_tests.exe',[objects['src/wardrobe_snapshot.cpp'],extra(root/'tests'/'wardrobe_snapshot.cpp')])
link(build/'wardrobe_selection_tests.exe',[objects['src/wardrobe_selection.cpp'],extra(root/'tests'/'wardrobe_selection.cpp')])
link(build/'wardrobe_transport_tests.exe',[objects['src/wardrobe_transport.cpp'],extra(root/'tests'/'wardrobe_transport.cpp')])
link(build/'render_preview.exe',[extra(root/'tools'/'render_preview.cpp')])
print(json.dumps({'status':'built','asi':str(out/'HexAppeal.asi'),'bytes':(out/'HexAppeal.asi').stat().st_size,'sha256':hashlib.sha256((out/'HexAppeal.asi').read_bytes()).hexdigest(),'compiler':compiler},indent=2))
