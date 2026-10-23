import json
from pathlib import Path
import shutil
import subprocess
import time
from project import atomic_write


class Deployment:
    def __init__(self, root):
        self.root=Path(root)
        self.folder=self.root/'output/updates'
        self.folder.mkdir(parents=True,exist_ok=True,mode=0o700)
        self.journal=self.folder/'transaction.json'

    def read(self):
        if not self.journal.exists():return None
        return json.loads(self.journal.read_text())

    def save(self, state):
        atomic_write(self.journal,json.dumps(state,indent=2)+'\n')

    def begin(self, names, git_before=None, git_after=None):
        if self.read():raise RuntimeError('An interrupted installation must be recovered first.')
        backup=self.folder/('rollback-'+str(time.time_ns()));backup.mkdir(mode=0o700)
        state={'backup':str(backup),'files':{},'git_before':git_before,'git_after':git_after,'activation_attempted':False}
        for name in sorted(set(names)):
            path=self.root/name
            if path.is_symlink():state['files'][name]={'symlink':str(path.readlink())}
            elif path.is_file():
                destination=backup/name;destination.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(path,destination);state['files'][name]={'file':True}
            else:state['files'][name]={'absent':True}
        self.save(state)
        return state

    def activation(self):
        state=self.read();state['activation_attempted']=True;self.save(state)

    def rollback(self, setup):
        state=self.read()
        if not state:return False
        backup=Path(state['backup'])
        if backup.parent!=self.folder or not backup.name.startswith('rollback-'):
            raise RuntimeError('Invalid recovery record; saved files were retained for manual recovery.')
        for name, original in state['files'].items():
            if Path(name).is_absolute() or '..' in Path(name).parts:raise RuntimeError('Invalid recovery path.')
            destination=self.root/name
            destination.parent.mkdir(parents=True,exist_ok=True)
            if original.get('symlink'):
                temporary=destination.with_name(destination.name+'.restore');temporary.unlink(missing_ok=True)
                temporary.symlink_to(original['symlink']);temporary.replace(destination)
            elif original.get('file'):
                temporary=destination.with_name(destination.name+'.restore');shutil.copy2(backup/name,temporary);temporary.replace(destination)
            else:destination.unlink(missing_ok=True)
        if state.get('git_before'):
            current=subprocess.check_output(['git','-C',str(self.root),'rev-parse','HEAD'],text=True).strip()
            if current==state['git_after']:
                subprocess.run(['git','-C',str(self.root),'update-ref','HEAD',state['git_before'],state['git_after']],check=True)
                subprocess.run(['git','-C',str(self.root),'read-tree',state['git_before']],check=True)
            elif current!=state['git_before']:raise RuntimeError('Source history changed during installation; recovery needs review.')
        if state['activation_attempted'] and (self.root/'build-desktop/install-manifest.json').exists():
            setup([str(self.root/'.venv/bin/python'),str(self.root/'install.py')])
        self.journal.unlink()
        return True

    def finish(self, release):
        atomic_write(self.folder/'installed-release.json',json.dumps(release,indent=2)+'\n')
        self.journal.unlink(missing_ok=True)


def activation_files():
    return ['build-desktop/install-manifest.json','build-desktop/effect.json',
            'build-desktop/truehdr-desktop-session','build-desktop/desktop_input.spv','build/truehdr-panel',
            'output/updates/installed-release.json']
