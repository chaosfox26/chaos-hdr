
import hashlib
import json
import subprocess
from sdk import ROOT, fetch_sdk


def prepare(developer_checks=False):
    fetch_sdk()
    version=subprocess.check_output(['kwin_wayland','--version'],text=True).strip()
    digest=hashlib.sha256((version+'\n'+str(ROOT)).encode())
    sources=[*sorted((ROOT/'native/desktop').glob('*')),
             ROOT/'native/vulkan_hdr.cpp',ROOT/'native/vulkan_hdr.hpp',ROOT/'native/profiles.hpp']
    for path in sources:
        if path.is_file():digest.update(path.name.encode());digest.update(path.read_bytes())
    digest.update(b'development' if developer_checks else b'production')
    effect='truehdr_desktop_'+digest.hexdigest()[:12]
    build=ROOT/'build-desktop'
    subprocess.run(['cmake','-S',str(ROOT/'native/desktop'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release',
                    '-DTRUEHDR_EFFECT_ID='+effect,'-DTRUEHDR_DEVELOPMENT='+('ON' if developer_checks else 'OFF')],check=True)
    targets=['truehdr_desktop','truehdr-desktop-session','truehdr_bootstrap']
    if developer_checks:targets.extend(['truehdr-desktop-gpu-test','truehdr-desktop-window-test'])
    subprocess.run(['cmake','--build',str(build),'--target',*targets,'--parallel','4'],check=True)
    subprocess.run(['cmake','-S',str(ROOT),'-B',str(ROOT/'build'),'-DCMAKE_BUILD_TYPE=Release','-DTRUEHDR_DEVELOPMENT='+('ON' if developer_checks else 'OFF')],check=True)
    subprocess.run(['cmake','--build',str(ROOT/'build'),'--target','truehdr-panel','--parallel','4'],check=True)
    manifest={'route':'desktop','effect_id':effect,'kwin_version':version,'root':str(ROOT),
              'input':'direct-dmabuf','runtime':'1.2.3','debanding_default':True,'development_checks':developer_checks}
    (build/'install-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Built the Chaos HDR direct-buffer KDE route. No games were launched.')

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description='Build the native KDE Chaos HDR route.')
    parser.add_argument('--developer-checks',action='store_true')
    prepare(parser.parse_args().developer_checks)
