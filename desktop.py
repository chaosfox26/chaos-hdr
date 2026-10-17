DESCRIPTION = 'Load and supervise the Chaos HDR KDE effect without launching or modifying games.'
import argparse
import fcntl
import json
import os
from pathlib import Path
import re
import subprocess
import time
from project import ROOT, atomic_write

PREFIX='truehdr_desktop'
SCRIPT='truehdr-desktop-loader'

def call(path,method,*arguments,service='org.kde.KWin'):
    result=subprocess.run(['qdbus6',service,path,method,*arguments],check=True,
                          capture_output=True,text=True,timeout=8)
    return result.stdout.strip()

def manifest():
    data=json.loads((ROOT/'build-desktop/install-manifest.json').read_text())
    current=subprocess.check_output(['kwin_wayland','--version'],text=True).strip()
    if data['kwin_version']!=current or Path(data['root'])!=ROOT:
        raise RuntimeError('KWin or the project location changed. Run the installer again to rebuild the effect.')
    support=call('/KWin','org.kde.KWin.supportInformation')
    running=re.search(r'^KWin version:\s*(\S+)',support,re.M)
    if not running or current.split()[-1]!=running[1]:
        raise RuntimeError('The running KWin differs from the installed build. Log out and back in before loading the rebuilt effect.')
    return data

def start():
    folder=ROOT/'output/desktop';folder.mkdir(parents=True,exist_ok=True)
    with (folder/'attach.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        return attach()

def attach():
    effect=manifest()['effect_id']
    loaded=call('/Effects','org.kde.kwin.Effects.loadedEffects').splitlines()
    if effect in loaded:return effect
    if call('/Scripting','org.kde.kwin.Scripting.isScriptLoaded',SCRIPT)!='true':
        number=call('/Scripting','org.kde.kwin.Scripting.loadDeclarativeScript',str(ROOT/'native/desktop/loader.qml'),SCRIPT)
        if not number.isdigit():raise RuntimeError('KDE could not register the HDR effect path.')
        call('/Scripting/Script'+number,'org.kde.kwin.Script.run')
    old=[name for name in loaded if name.startswith(PREFIX)]
    for name in old:call('/Effects','org.kde.kwin.Effects.unloadEffect',name)
    try:
        if call('/Effects','org.kde.kwin.Effects.loadEffect',effect)!='true':raise RuntimeError('KDE could not load the HDR effect.')
    except Exception:
        for name in old:call('/Effects','org.kde.kwin.Effects.loadEffect',name)
        raise
    return effect

def stop():
    for name in call('/Effects','org.kde.kwin.Effects.loadedEffects').splitlines():
        if name.startswith(PREFIX):call('/Effects','org.kde.kwin.Effects.unloadEffect',name)
    if call('/Scripting','org.kde.kwin.Scripting.isScriptLoaded',SCRIPT)=='true':
        call('/Scripting','org.kde.kwin.Scripting.unloadScript',SCRIPT)

def refresh_display():


    from displays import scan
    profile=json.loads(Path(os.environ.get('TRUEHDR_PROFILE',str(ROOT/'profile.json'))).read_text())
    candidates=scan();display=next((d for d in candidates if d['connector']==profile.get('target_display')),None)
    if display is None:display=next(iter(candidates),None)
    if not display:return
    peak=display['peak_brightness']
    if not 400<=peak<=2000:raise RuntimeError('The OS HDR peak is outside the runtime’s supported 400–2000 nit range.')
    profile.update(peak_brightness=peak,target_display=display['connector'],peak_source=display['source'])
    folder=Path(os.environ.get('TRUEHDR_DESKTOP_TEST_ROOT',str(ROOT)))/'output';folder.mkdir(exist_ok=True)
    atomic_write(folder/'worker-results.json',json.dumps({'last_profile':profile,'route':'desktop','frames':0,
                 'updated':time.time(),'scope':'Display metadata only; no Python GPU worker'})+'\n')

def daemon():
    controller=ROOT/'build-desktop/truehdr-desktop-session'
    if not controller.is_file():
        raise RuntimeError('The event-driven session controller is missing. Run the installer to build it.')
    os.execv(str(controller),[str(controller),str(ROOT)])

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=DESCRIPTION)
    action=parser.add_mutually_exclusive_group()
    action.add_argument('--daemon',action='store_true');action.add_argument('--stop',action='store_true')
    action.add_argument('--controls',action='store_true')
    args=parser.parse_args()
    try:
        if args.daemon:daemon()
        elif args.stop:stop()
        else:
            effect=start();refresh_display()
            if args.controls:atomic_write(ROOT/'overlay.toggle',str(time.time_ns()))
            print('Chaos HDR direct-buffer KDE route ready.')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as exc:parser.exit(1,str(exc)+'\n')
