DESCRIPTION = 'Install Chaos HDR desktop entry and session services without changing game files.'
import argparse
from pathlib import Path
import subprocess

from project import ROOT, SETTINGS, atomic_write
import json

UNITS = Path.home() / '.config/systemd/user'
DESKTOP = Path.home() / '.local/share/applications/truehdr-linux.desktop'
UNINSTALL_COMMAND = Path.home() / '.local/bin/chaos-hdr-uninstall'


def install():
    if (UNINSTALL_COMMAND.exists() or UNINSTALL_COMMAND.is_symlink()) and UNINSTALL_COMMAND.resolve() != ROOT/'uninstall.sh':
        raise RuntimeError('The uninstall command name is already used by another file. Installation was not changed.')
    panel=ROOT/'build/truehdr-panel'
    from desktop import manifest
    info=manifest()
    required=[ROOT/'.venv/bin/python',panel,ROOT/'uninstall.sh',ROOT/'build-desktop/truehdr-desktop-session',
              ROOT/'build-desktop/bootstrap/libtruehdr_bootstrap.so',
              ROOT/'build-desktop/kwin/effects/plugins'/f'{info["effect_id"]}.so']
    for file in required:
        if not file.exists():
            raise RuntimeError('Build Chaos HDR and install its Python dependencies first.')
    if not (ROOT / 'profile.json').exists():
        atomic_write(ROOT / 'profile.json', (ROOT / 'profile.example.json').read_text())
    for name in ('profile.json','profiles.json','window-targets.json','overlay.toggle'):
        if (ROOT/name).is_file():(ROOT/name).chmod(0o600)
    UNITS.mkdir(parents=True, exist_ok=True)
    specs = {
        'truehdr-overlay-controls': f'"{ROOT}/build/truehdr-panel" "{ROOT}/profile.json" --overlay-service',
    }
    specs['truehdr-desktop']=f'"{ROOT}/.venv/bin/python" "{ROOT}/desktop.py" --daemon'
    for name in ('truehdr-worker','truehdr-native-worker'):
        subprocess.run(['systemctl','--user','disable','--now',name],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    for name in specs:
        subprocess.run(['systemctl', '--user', 'stop', name], check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        subprocess.run(['systemctl', '--user', 'reset-failed', name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        (UNITS / (name + '.service')).write_text(
            '[Unit]\nDescription=Chaos HDR ' + name + '\nAfter=graphical-session.target\nPartOf=graphical-session.target\n'
            '[Service]\nType=simple\nWorkingDirectory=' + str(ROOT) + '\nExecStart=' + specs[name] + '\n'
            'Environment=QT_QPA_PLATFORM=wayland\nRestart=on-failure\nRestartSec=2\nTimeoutStopSec=5\n'
            '[Install]\nWantedBy=graphical-session.target\n')
    subprocess.run(['systemctl', '--user', 'daemon-reload'], check=True)
    subprocess.run(['systemctl', '--user', 'enable', '--now', *specs], check=True)
    DESKTOP.parent.mkdir(parents=True, exist_ok=True)
    DESKTOP.write_text('[Desktop Entry]\nType=Application\nName=Chaos HDR\nComment=Game HDR and sharpening controls\n'
                       f'Exec="{panel}" "{ROOT}/profile.json"\nIcon=video-display\nTerminal=false\nCategories=Game;\n')
    UNINSTALL_COMMAND.parent.mkdir(parents=True,exist_ok=True)
    if not UNINSTALL_COMMAND.is_symlink():UNINSTALL_COMMAND.symlink_to(ROOT/'uninstall.sh')
    atomic_write(SETTINGS,json.dumps({'route':'desktop'})+'\n')
    from desktop import start,refresh_display
    start();refresh_display()
    old=Path.home()/'.local/lib/qt6/plugins/kwin/effects/plugins/truehdr_desktop.so'
    if old.is_symlink() and old.resolve().is_relative_to(ROOT):old.unlink()
    print('Installed Chaos HDR. KDE filtering starts automatically; Alt+Z opens controls. Uninstall: ~/.local/bin/chaos-hdr-uninstall. No game was launched.')


def uninstall():
    transient=('chaos-hdr-update','chaos-hdr-release-check')
    for name in transient:
        subprocess.run(['systemctl','--user','stop',name],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    services=('truehdr-worker','truehdr-native-worker','truehdr-overlay-controls','truehdr-desktop')
    for name in services:
        subprocess.run(['systemctl','--user','disable','--now',name],check=False,
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    for name in (*services,*transient):
        state=subprocess.run(['systemctl','--user','is-active','--quiet',name],check=False)
        if state.returncode not in (3,4):
            raise RuntimeError('An HDR service could not be stopped. Files were retained; retry uninstall after stopping it.')
    from desktop import stop
    try:stop()
    except subprocess.SubprocessError:
        available=subprocess.run(['qdbus6','org.kde.KWin'],capture_output=True,check=False)
        if available.returncode==0:
            raise RuntimeError('KDE could not unload the HDR effect. Files were retained; log out and retry uninstall.')
    panel=ROOT/'build/truehdr-panel'
    if panel.is_file():subprocess.run([str(panel),'--unregister-shortcut'],check=True,
                                    stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    for name in services:(UNITS/(name+'.service')).unlink(missing_ok=True)
    subprocess.run(['systemctl','--user','daemon-reload'],check=True)
    DESKTOP.unlink(missing_ok=True)
    for command in (UNINSTALL_COMMAND,UNINSTALL_COMMAND.with_name('truehdr-uninstall')):
        if command.is_symlink() and command.resolve()==ROOT/'uninstall.sh':command.unlink()
    old=Path.home()/'.local/lib/qt6/plugins/kwin/effects/plugins/truehdr_desktop.so'
    if old.is_symlink() and old.resolve().is_relative_to(ROOT):old.unlink()
    SETTINGS.unlink(missing_ok=True)
    (ROOT/'overlay.toggle').unlink(missing_ok=True)
    print('Chaos HDR uninstalled: services, KDE effect, shortcut and menu registration removed. Saved profiles and project files retained. Shared system packages were not removed.')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument('--uninstall', action='store_true')
    args=parser.parse_args()
    try:
        uninstall() if args.uninstall else install()
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        parser.exit(1, f'{exc}\n')
