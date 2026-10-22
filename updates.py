import argparse
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

from project import ROOT, atomic_write

REPOSITORY='chaosfox26/chaos-hdr'
API='https://api.github.com/repos/'+REPOSITORY
DOWNLOAD='https://github.com/'+REPOSITORY+'/releases/download/'
DAY=24*60*60
INSTALLER='Chaos-HDR-CachyOS.run'


def read(path):
    try:
        value=json.loads(path.read_text())
        return value if isinstance(value,dict) else {}
    except (OSError,ValueError):return {}


def version(tag,prerelease=False):
    match=re.fullmatch(r'v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?',tag)
    if not match:raise ValueError('Unsupported release version')
    parts=match[4].split('.') if match[4] else []
    if any(not p or (p.isdigit() and len(p)>1 and p.startswith('0')) for p in parts):
        raise ValueError('Unsupported prerelease version')
    identifiers=tuple((0,int(p)) if p.isdigit() else (1,p) for p in parts)
    return (*map(int,match.group(1,2,3)),0 if parts or prerelease else 1,identifiers)


def installer(release):
    tag=release.get('tag_name','')
    version(tag,release.get('prerelease',False))
    expected=DOWNLOAD+tag+'/'+INSTALLER
    asset=next((a for a in release.get('assets',[]) if a.get('name')==INSTALLER),{})
    digest=asset.get('digest','') or ''
    if asset.get('browser_download_url')!=expected or not re.fullmatch(r'sha256:[0-9a-f]{64}',digest):
        raise ValueError('This release has no supported installer with a SHA-256 digest.')
    return expected,digest.removeprefix('sha256:')


def choose(releases,current):
    installed=version(current['tag'],current.get('prerelease',False))
    candidates=[]
    for release in releases:
        if release.get('draft'):continue
        try:latest=version(release['tag_name'],release.get('prerelease',False))
        except (KeyError,ValueError,TypeError):continue
        if not current.get('prerelease',False) and latest[3]==0:continue
        if latest>installed:
            candidates.append((latest,release))
    return max(candidates,key=lambda pair:pair[0])[1] if candidates else None


def fetch(url):
    request=urllib.request.Request(url,headers={'Accept':'application/vnd.github+json','User-Agent':'Chaos-HDR-release-updater','X-GitHub-Api-Version':'2022-11-28'})
    with urllib.request.urlopen(request,timeout=20) as response:
        data=response.read(4*1024*1024+1)
    if len(data)>4*1024*1024:raise ValueError('GitHub release response is too large.')
    return json.loads(data)


def published_releases():
    releases=[]
    for page in range(1,11):
        batch=fetch(API+f'/releases?per_page=100&page={page}')
        if not isinstance(batch,list):raise ValueError('GitHub returned an invalid release list.')
        releases.extend(batch)
        if len(batch)<100:return releases
    raise ValueError('Too many releases to check safely; try again later.')


def notify(title,body):
    try:
        result=subprocess.run(['notify-send','--app-name=Chaos HDR','--icon=system-software-update',title,body],
                              stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
        return result.returncode==0
    except (OSError,subprocess.SubprocessError):return False


class Updater:
    def __init__(self,root=ROOT):
        self.root=Path(root)
        self.folder=self.root/'output/updates'
        self.folder.mkdir(parents=True,exist_ok=True,mode=0o700)
        self.path=self.folder/'status.json'
        self.current=read(self.root/'release.json')
        version(self.current['tag'],self.current.get('prerelease',False))

    @contextlib.contextmanager
    def lock(self):
        with (self.folder/'operation.lock').open('a') as lock:
            os.fchmod(lock.fileno(),0o600)
            try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            except BlockingIOError:yield False;return
            try:yield True
            finally:fcntl.flock(lock,fcntl.LOCK_UN)

    def save(self,state):
        state.update(current_tag=self.current['tag'],prerelease=self.current.get('prerelease',False))
        atomic_write(self.path,json.dumps(state,indent=2)+'\n')

    def check(self,manual=False,now=None):
        now=time.time() if now is None else now
        with self.lock() as acquired:
            if not acquired:return read(self.path)
            state=read(self.path)
            last=state.get('last_attempt',0)
            if not manual and last and now-last<DAY:
                if state.get('current_tag')!=self.current['tag']:
                    available=state.get('available') or {}
                    if available and version(available['tag'],available.get('prerelease',False))<=version(self.current['tag'],self.current.get('prerelease',False)):
                        state.update(available=None,status='current',message='No newer release is published.')
                    self.save(state)
                return state
            state.update(last_attempt=now,status='checking',message='Checking GitHub releases...',busy=False,error='')
            self.save(state)
            try:
                releases=published_releases()
                release=choose(releases,self.current)
                state.update(last_success=now,available=None)
                if release:
                    installer(release)
                    state.update(status='available',available={'tag':release['tag_name'],'prerelease':release.get('prerelease',False)},message='Chaos HDR has an update: '+release['tag_name'])
                    if state.get('notified_tag')!=release['tag_name'] and notify('Chaos HDR has an update',release['tag_name']+' is available. Open Alt+Z, then Updates.'):
                        state['notified_tag']=release['tag_name']
                else:
                    state.update(status='current' if releases else 'unpublished',message='No newer release is published.' if releases else 'No published release yet.')
            except (OSError,ValueError,KeyError,TypeError) as exc:
                state.update(status='error',error='Could not check GitHub releases. Check your connection and try again.',message='Update check unavailable.')
                if isinstance(exc,urllib.error.HTTPError) and exc.code in (403,429):state['error']='GitHub temporarily limited update checks. Try again later.'
                elif isinstance(exc,ValueError):state['error']=str(exc)
            self.save(state)
            return state

    def git(self,*arguments):
        return subprocess.check_output(['git','-C',str(self.root),*arguments],text=True,stderr=subprocess.STDOUT,timeout=60).strip()

    def source_preflight(self):
        if not (self.root/'.git').exists():return False
        if self.git('status','--porcelain','--untracked-files=normal'):
            raise RuntimeError('Local source changes are present. Commit or back them up before installing a release. Rebuild is still available.')
        remote=self.git('remote','get-url','origin').rstrip('/').removesuffix('.git')
        if remote!='https://github.com/'+REPOSITORY and remote.partition('@')!=('git','@','github.com:'+REPOSITORY):
            raise RuntimeError('This source checkout has a different origin. Update it manually, then use Rebuild.')
        return True

    def download(self,release,folder):
        url,digest=installer(release)
        path=folder/INSTALLER
        request=urllib.request.Request(url,headers={'User-Agent':'Chaos-HDR-release-updater'})
        checksum=hashlib.sha256()
        with urllib.request.urlopen(request,timeout=30) as response,path.open('wb') as output:
            if not response.url.startswith('https://'):raise RuntimeError('An insecure download redirect was refused.')
            size=0
            while chunk:=response.read(1024*1024):
                size+=len(chunk)
                if size>128*1024*1024:raise RuntimeError('The installer download is too large.')
                checksum.update(chunk);output.write(chunk)
        if checksum.hexdigest()!=digest:raise RuntimeError('Installer verification failed. Nothing was installed.')
        return path

    def install(self,rebuild=False):
        with self.lock() as acquired:
            if not acquired:return 1
            state=read(self.path)
            state.update(busy=True,status='installing',error='',message='Rebuilding Chaos HDR...' if rebuild else 'Preparing release update...')
            self.save(state)
            code=1
            try:
                if rebuild:
                    command=['bash',str(self.root/'bootstrap.sh')]
                    self.run_setup(command)
                else:
                    source=self.source_preflight()
                    releases=published_releases()
                    release=choose(releases,self.current)
                    if not release:raise RuntimeError('No newer applicable release is available.')
                    installer(release)
                    scratch=self.root/'output/scratch';scratch.mkdir(parents=True,exist_ok=True)
                    with tempfile.TemporaryDirectory(prefix='release-update-',dir=scratch) as stage:
                        stage=Path(stage)
                        payload=self.download(release,stage)
                        if source:
                            tag=release['tag_name']
                            self.git('fetch','--no-tags','origin','refs/tags/'+tag)
                            target=self.git('rev-parse','FETCH_HEAD^{commit}')
                            self.source_preflight()
                            try:self.git('merge-base','--is-ancestor','HEAD',target)
                            except subprocess.CalledProcessError:raise RuntimeError('This checkout contains work beyond the release. Update it manually; no local commits were replaced.')
                            self.git('merge','--ff-only',target)
                            self.run_setup(['bash',str(self.root/'bootstrap.sh')])
                        else:
                            from package_installer import SOURCE_FILES
                            backup=self.folder/('source-backup-'+str(time.time_ns()))
                            for name in (*SOURCE_FILES,'LICENSE'):
                                original=self.root/name
                                if original.is_file():
                                    destination=backup/name;destination.parent.mkdir(parents=True,exist_ok=True)
                                    shutil.copy2(original,destination)
                            self.run_setup(['bash',str(payload),str(self.root)])
                installed=read(self.root/'release.json')
                version(installed['tag'],installed.get('prerelease',False))
                if not rebuild and installed!={'tag':release['tag_name'],'prerelease':release.get('prerelease',False)}:
                    raise RuntimeError('Setup did not report the requested release version. Check output/updates/install.log before retrying.')
                self.current=installed
                available=state.get('available') if rebuild else None
                state.update(status='available' if available else 'complete',available=available,
                             message='Rebuild complete. Chaos HDR has an update: '+available['tag'] if available else 'Chaos HDR is ready. Open Alt+Z to continue.')
                notify('Chaos HDR is ready','The rebuild finished.' if rebuild else 'The release update finished.')
                code=0
            except (OSError,ValueError,KeyError,TypeError,RuntimeError,subprocess.SubprocessError) as exc:
                detail=str(exc) if isinstance(exc,RuntimeError) else 'Setup failed. See output/updates/install.log, resolve the issue, then try Rebuild.'
                if isinstance(exc,urllib.error.URLError):detail='Could not reach GitHub. Check your connection and try the update again.'
                state.update(status='error',message='Chaos HDR setup did not complete.',error=detail)
                notify('Chaos HDR setup needs attention',detail)
            state['busy']=False
            self.save(state)
            return code

    def run_setup(self,command):
        with (self.folder/'install.log').open('w') as log:
            os.fchmod(log.fileno(),0o600)
            result=subprocess.run(command,cwd=self.root,stdout=log,stderr=subprocess.STDOUT)
        if result.returncode:raise RuntimeError('Setup failed. See output/updates/install.log, resolve the issue, then try Rebuild.')


def main():
    parser=argparse.ArgumentParser(description='Check GitHub releases or explicitly update/rebuild Chaos HDR.')
    choice=parser.add_mutually_exclusive_group(required=True)
    choice.add_argument('--check',action='store_true')
    choice.add_argument('--install',action='store_true')
    choice.add_argument('--rebuild',action='store_true')
    parser.add_argument('--manual',action='store_true')
    args=parser.parse_args()
    updater=Updater()
    if args.check:
        updater.check(args.manual)
        return 0
    return updater.install(args.rebuild)


if __name__=='__main__':
    raise SystemExit(main())
