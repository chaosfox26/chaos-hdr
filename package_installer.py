DESCRIPTION = 'Create a Chaos HDR source installer with optional personal picture profiles.\n\nDependencies are fetched by bootstrap from their publishers, not redistributed.\nNo game files, account data, personal captures, environment or build caches are packed.\n'
import argparse
import hashlib
import gzip
import io
import re
import json
import shlex
import tarfile
from urllib.parse import urlsplit
from project import ROOT

HEADER='''#!/usr/bin/env bash
set -euo pipefail
task_payload_line=$(awk '/^__TRUEHDR_ARCHIVE__$/ {print NR+1; exit}' "$0")
task_expected=PAYLOAD_SHA256
if [[ "${1:-}" == --check ]]; then
    task_actual=$(tail -n +"$task_payload_line" "$0" | sha256sum | cut -d ' ' -f1)
    [[ "$task_actual" == "$task_expected" ]] || { echo "Installer integrity check failed."; exit 1; }
    echo "Installer integrity verified."
    exit 0
fi
task_extract_only=0
if [[ "${1:-}" == --extract-only ]]; then task_extract_only=1; shift; fi
task_target="${1:-$HOME/Documents/ChatGPT/CachyOS/RTX HDR experimental}"
if [[ "$(basename -- "$task_target")" != 'RTX HDR experimental' ]]; then
    echo "Choose a destination folder named RTX HDR experimental."
    exit 1
fi
mkdir -p "$task_target/output/scratch"
task_target="$(cd -- "$task_target" && pwd)"
task_stage=$(mktemp -d "$task_target/output/scratch/install.XXXXXX")
trap 'rm -rf -- "$task_stage"' EXIT
tail -n +"$task_payload_line" "$0" > "$task_stage/payload.tar.gz"
task_actual=$(sha256sum "$task_stage/payload.tar.gz" | cut -d ' ' -f1)
[[ "$task_actual" == "$task_expected" ]] || { echo "Installer integrity check failed."; exit 1; }
mkdir "$task_stage/source"
tar -xzf "$task_stage/payload.tar.gz" -C "$task_stage/source"
for task_profile in profile.json profiles.json launch-settings.json window-targets.json; do
    if [[ -e "$task_target/$task_profile" ]]; then rm -f -- "$task_stage/source/$task_profile"; fi
done
if [[ $task_extract_only -eq 0 && -x "$task_target/.venv/bin/python" && -f "$task_target/build-desktop/install-manifest.json" ]]; then
    mkdir "$task_stage/RTX HDR experimental"
    cp -a "$task_stage/source/." "$task_stage/RTX HDR experimental/"
    "$task_target/.venv/bin/python" "$task_stage/RTX HDR experimental/local_setup.py" "$task_target" "$task_stage/RTX HDR experimental"
    exit 0
fi
cp -a "$task_stage/source/." "$task_target/"
if [[ $task_extract_only -eq 1 ]]; then echo "Source extracted; existing profiles preserved."; exit 0; fi
cd "$task_target"
bash ./bootstrap.sh
exit 0
__TRUEHDR_ARCHIVE__
'''

SOURCE_FILES = (
    '.gitignore','AGENTS.md','CMakeLists.txt','README.md','Chaos HDR development.md','THIRD_PARTY_NOTICES.rst',
    'docs/installation.md','docs/user-guide.md','docs/architecture.md','docs/compatibility.md',
    'docs/performance.md','docs/privacy.md','docs/troubleshooting.md','docs/contributing.md','docs/credits.md','docs/releases/0.2.0.md','docs/releases/0.4.0.md','docs/releases/0.4.1.md','docs/releases/0.4.2.md',
    'docs/images/chaos-hdr-sidebar.png','docs/images/chaos-hdr-title.svg',
    'docs/images/download-installer.svg','docs/images/user-guide.svg','docs/images/neon-divider.svg',
    'bootstrap.sh','uninstall.sh','github-install.sh.in','desktop.py','desktop_probe.py','displays.py','install.py',
    'package_installer.py','prepare_desktop.py','project.py','sdk.py','test_desktop_watch.py','test_desktop_version.py','test_updates.py','test_install.py','test_package_installer.py','test_deployment.py','profile.example.json',
    'local_setup.py','deployment.py','updates.py','release.json','native/update_dialog.hpp',
    'native/panel.cpp','native/accessible_controls.hpp','native/picture_settings.hpp','native/profiles.hpp','native/test_profiles.cpp','native/vulkan_hdr.cpp','native/vulkan_hdr.hpp',
    'native/desktop/CMakeLists.txt','native/desktop/bootstrap.cpp','native/desktop/effect.cpp',
    'native/desktop/effect.json.in','native/desktop/gpu.cpp','native/desktop/gpu.hpp','native/desktop/gpu_test.cpp',
    'native/desktop/input.comp','native/desktop/loader.qml','native/desktop/presentation_metrics.hpp',
    'native/desktop/session.cpp','native/desktop/update_controller.hpp','native/desktop/window_test.cpp',
    'native/desktop/font_fallback.hpp','native/desktop/font_texture_test.cpp','native/desktop/overlay_gl.hpp','native/desktop/overlay_motion.hpp','native/desktop/overlay_style.hpp','native/desktop/overlay_metrics.hpp','native/test_overlay_motion.cpp',
    'assets/chaos-lightning.png','assets/ARTWORK.json',
    'vendor/fonts/Barlow-Regular.ttf','vendor/fonts/Barlow-SemiBold.ttf','vendor/fonts/RubikGlitch-Regular.ttf',
    'vendor/fonts/Audiowide-Regular.ttf','vendor/fonts/OFL-Audiowide.txt',
    'vendor/fonts/OFL-Barlow.txt','vendor/fonts/OFL-RubikGlitch.txt','vendor/fonts/UPSTREAM.json',
    'vendor/imgui/LICENSE.txt','vendor/imgui/UPSTREAM.txt','vendor/imgui/imconfig.h',
    'vendor/imgui/imgui.h','vendor/imgui/imgui_internal.h','vendor/imgui/imgui.cpp',
    'vendor/imgui/imgui_draw.cpp','vendor/imgui/imgui_widgets.cpp','vendor/imgui/imgui_tables.cpp',
    'vendor/imgui/imstb_rectpack.h','vendor/imgui/imstb_textedit.h','vendor/imgui/imstb_truetype.h',
    'vendor/imgui/backends/imgui_impl_opengl3.cpp','vendor/imgui/backends/imgui_impl_opengl3.h',
    'vendor/imgui/backends/imgui_impl_opengl3_loader.h',
)


def privacy_check(files):
    patterns=(rb'/(?:home|Users)/[^/\s"\x27]+',rb'(?i)(?:ghp_|github_pat_|sk-proj-)[A-Za-z0-9_]{12,}')
    for name,data in files.items():
        if any(re.search(pattern,data) for pattern in patterns):
            raise RuntimeError('Public privacy check rejected a local path or credential pattern in '+name)
        if not name.startswith('vendor/') and re.search(rb'[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}',data):
            raise RuntimeError('Public privacy check rejected an email address in '+name)


def release_url(value):
    parsed=urlsplit(value)
    if parsed.scheme!='https' or parsed.netloc!='github.com' or parsed.query or parsed.fragment or not re.fullmatch(r'/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/releases/download/[^/]+/Chaos-HDR-CachyOS\.run',parsed.path):
        raise argparse.ArgumentTypeError('Use the exact HTTPS GitHub release URL for Chaos-HDR-CachyOS.run.')
    return value


def build(personal=False, installer_url=None):
    if installer_url:
        release_url(installer_url)
        if personal:raise ValueError('Personal backups cannot generate a public GitHub installer.')
    files={name:(ROOT/name).read_bytes() for name in SOURCE_FILES}
    if not (ROOT/'LICENSE').is_file():raise RuntimeError('The public source license is required for packaging.')
    for name in ('LICENSE','LICENSE.txt','COPYING','NOTICE'):
        if (ROOT/name).is_file():files[name]=(ROOT/name).read_bytes()
    privacy_check(files)
    if personal:
        for name in ('profile.json','profiles.json','window-targets.json'):
            if (ROOT/name).is_file():
                value=json.loads((ROOT/name).read_text())
                files[name]=(json.dumps(value,indent=2)+'\n').encode()
    manifest={'format':1,'release':json.loads((ROOT/'release.json').read_text()),'default_route':'desktop','personal_profiles':personal,'production_capture':False,'source_allowlist':True,
              'dependencies':'Fetched separately from their publishers by bootstrap.sh',
              'files':{name:hashlib.sha256(data).hexdigest() for name,data in sorted(files.items())}}
    files['INSTALL-MANIFEST.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    payload=io.BytesIO()
    with tarfile.open(fileobj=payload,mode='w') as archive:
        for name,data in sorted(files.items()):
            entry=tarfile.TarInfo(name);entry.size=len(data);entry.mode=0o600 if name in ('profile.json','profiles.json','window-targets.json') else 0o755 if name.endswith('.sh') else 0o644
            archive.addfile(entry,io.BytesIO(data))
    data=gzip.compress(payload.getvalue(),mtime=0);digest=hashlib.sha256(data).hexdigest()
    folder=ROOT/'output/installers';folder.mkdir(parents=True,exist_ok=True)
    name='Chaos-HDR-CachyOS'+('-personal' if personal else '')
    script=folder/(name+'.run');script.write_bytes(HEADER.replace('PAYLOAD_SHA256',digest).encode()+data);script.chmod(0o700 if personal else 0o755)
    source=folder/(name+'-source.tar.gz');source.write_bytes(data);source.chmod(0o600 if personal else 0o644)
    (folder/(name+'.sha256')).write_text(hashlib.sha256(script.read_bytes()).hexdigest()+'  '+script.name+'\n')
    if installer_url:
        launcher=(ROOT/'github-install.sh.in').read_text().replace('@INSTALLER_URL@',shlex.quote(installer_url)).replace('@INSTALLER_SHA256@',hashlib.sha256(script.read_bytes()).hexdigest())
        (folder/'install.sh').write_text(launcher)
    print(json.dumps({'installer':str(script),'bytes':script.stat().st_size,'personal_profiles':personal}))
    return script

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=DESCRIPTION)
    choice=parser.add_mutually_exclusive_group()
    choice.add_argument('--personal',action='store_true')
    choice.add_argument('--public',action='store_true')
    parser.add_argument('--installer-url',type=release_url,help='Exact versioned GitHub release URL; also creates the download-and-install entry point.')
    args=parser.parse_args()
    if args.personal and args.installer_url:parser.error('--installer-url is only available for public packages')
    build(args.personal,args.installer_url)
