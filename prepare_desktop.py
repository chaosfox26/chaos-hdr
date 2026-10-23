import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sdk
from project import ROOT, atomic_write


def fingerprint(source, root, version, developer_checks, environment=""):
    digest=hashlib.sha256((version+'\n'+str(root)+'\n'+environment).encode())
    files=[source/'CMakeLists.txt',source/'release.json',source/'prepare_desktop.py',source/'sdk.py']
    for directory in ('native','assets','vendor'):
        files.extend(p for p in (source/directory).rglob('*') if p.is_file())
    for path in sorted(files):
        digest.update(str(path.relative_to(source)).encode()+b'\0');digest.update(path.read_bytes())
    digest.update(b'development' if developer_checks else b'production')
    return digest.hexdigest()


def publish(root, manifest):
    root=Path(root).resolve();artifacts=Path(manifest['artifacts']).resolve()
    if Path(manifest['root']).resolve()!=root or not artifacts.is_relative_to(root/'output/builds'):
        raise RuntimeError('The candidate artifacts do not belong to this installation.')
    required=['desktop/truehdr-desktop-session','desktop/desktop_input.spv','desktop/effect.json',
              'desktop/bootstrap/libtruehdr_bootstrap.so','desktop/bootstrap/qmldir','controls/truehdr-panel',
              'desktop/kwin/effects/plugins/'+manifest['effect_id']+'.so']
    if any(not (artifacts/name).is_file() for name in required):raise RuntimeError('The candidate build is incomplete.')
    folder=root/'build-desktop';folder.mkdir(exist_ok=True)
    registry=folder/'kwin/effects/plugins';registry.mkdir(parents=True,exist_ok=True)
    names={'truehdr-desktop-session':'desktop/truehdr-desktop-session',
           'desktop_input.spv':'desktop/desktop_input.spv'}
    if manifest['development_checks']:
        names.update({'truehdr-desktop-gpu-test':'desktop/truehdr-desktop-gpu-test',
                      'truehdr-desktop-window-test':'desktop/truehdr-desktop-window-test'})
    def link(target, destination):
        destination.parent.mkdir(parents=True,exist_ok=True)
        temporary=destination.with_name(destination.name+'.next')
        temporary.unlink(missing_ok=True);temporary.symlink_to(target);temporary.replace(destination)
    for name, target in names.items():link(artifacts/target,folder/name)
    link(artifacts/'desktop/kwin/effects/plugins'/(manifest['effect_id']+'.so'),registry/(manifest['effect_id']+'.so'))
    bootstrap=folder/'bootstrap';bootstrap.mkdir(exist_ok=True)
    for name in ('libtruehdr_bootstrap.so','qmldir'):
        if not (bootstrap/name).exists():shutil.copy2(artifacts/'desktop/bootstrap'/name,bootstrap/name)
    link(artifacts/'controls/truehdr-panel',root/'build/truehdr-panel')
    atomic_write(folder/'effect.json',(artifacts/'desktop/effect.json').read_text())
    atomic_write(folder/'install-manifest.json',json.dumps(manifest,indent=2)+'\n')


def prepare(developer_checks=False, runtime_root=None, stage_only=False):
    source=ROOT;root=Path(runtime_root).resolve() if runtime_root else source
    sdk.ROOT=root;sdk.fetch_sdk()
    version=subprocess.check_output(['kwin_wayland','--version'],text=True).strip()
    release=json.loads((source/'release.json').read_text())
    environment='\n'.join(subprocess.check_output(command,text=True) for command in (
        ['cmake','--version'],['c++','--version'],
        ['pkg-config','--modversion','Qt6Core','Qt6Quick','epoxy','libdrm','wayland-server','fontconfig','vulkan']))
    environment+='\n'+hashlib.sha256(Path('/usr/lib/libkwin.so').read_bytes()).hexdigest()
    environment+='\n'+repr({name:os.environ.get(name,'') for name in ('CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','CMAKE_PREFIX_PATH')})
    digest=fingerprint(source,root,version,developer_checks,environment)
    effect='truehdr_desktop_'+digest[:12]
    artifacts=root/'output/builds'/digest
    complete=artifacts/'manifest.json'
    if complete.is_file():manifest=json.loads(complete.read_text())
    else:
        if artifacts.exists():shutil.rmtree(artifacts)
        artifacts.mkdir(parents=True)
        for name in ('assets','vendor/fonts'):shutil.copytree(source/name,artifacts/name)
        build=artifacts/'desktop'
        subprocess.run(['cmake','-S',str(source/'native/desktop'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release',
                        '-DTRUEHDR_ROOT='+str(root),'-DTRUEHDR_ASSET_ROOT='+str(artifacts),
                        '-DTRUEHDR_EFFECT_ID='+effect,'-DTRUEHDR_DEVELOPMENT='+('ON' if developer_checks else 'OFF')],check=True)
        targets=['truehdr_desktop','truehdr-desktop-session','truehdr_bootstrap']
        if developer_checks:targets.extend(['truehdr-desktop-gpu-test','truehdr-desktop-window-test'])
        subprocess.run(['cmake','--build',str(build),'--target',*targets,'--parallel','4'],check=True)
        subprocess.run(['cmake','-S',str(source),'-B',str(artifacts/'controls'),'-DCMAKE_BUILD_TYPE=Release','-DTRUEHDR_DEVELOPMENT='+('ON' if developer_checks else 'OFF')],check=True)
        subprocess.run(['cmake','--build',str(artifacts/'controls'),'--target','truehdr-panel','--parallel','4'],check=True)
        manifest={'route':'desktop','effect_id':effect,'kwin_version':version,'release':release,'root':str(root),
                  'artifacts':str(artifacts),'fingerprint':digest,'input':'direct-dmabuf','runtime':'1.2.3',
                  'debanding_default':True,'development_checks':developer_checks}
        atomic_write(complete,json.dumps(manifest,indent=2)+'\n')
    if stage_only:
        (root/'output/updates').mkdir(parents=True,exist_ok=True)
        atomic_write(root/'output/updates/candidate.json',json.dumps(manifest,indent=2)+'\n')
    else:publish(root,manifest)
    print('Built the Chaos HDR direct-buffer KDE route. No games were launched.')
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description='Build the native KDE Chaos HDR route.')
    parser.add_argument('--developer-checks',action='store_true')
    parser.add_argument('--runtime-root',type=Path)
    parser.add_argument('--stage-only',action='store_true')
    args=parser.parse_args();prepare(args.developer_checks,args.runtime_root,args.stage_only)
