import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from deployment import Deployment, activation_files
from prepare_desktop import fingerprint, publish
from project import ROOT, atomic_write
from updates import Updater, read, DAY


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        (ROOT/'output/scratch').mkdir(parents=True,exist_ok=True)
        self.directory=tempfile.TemporaryDirectory(dir=ROOT/'output/scratch')
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)
        self.old={'tag':'v0.2.0','prerelease':True}
        self.new={'tag':'v0.3.0','prerelease':True}
        atomic_write(self.root/'release.json',json.dumps(self.old))
        (self.root/'profile.json').write_bytes(b'{"contrast":17}')
        self.updater=Updater(self.root)

    @patch('updates.published_releases')
    def test_stale_busy_and_checking_recover_without_violating_daily_limit(self, fetch):
        for status in ('installing','checking'):
            self.updater.save({'busy':status=='installing','status':status,'last_attempt':100000})
            state=self.updater.check(now=100001)
            self.assertFalse(state['busy']);self.assertEqual(state['status'],'interrupted')
        fetch.assert_not_called()

    @patch('updates.published_releases')
    def test_live_lock_prevents_interruption_recovery(self, fetch):
        self.updater.save({'busy':True,'status':'installing'})
        with self.updater.lock():
            state=Updater(self.root).check(manual=True)
            self.assertTrue(state['busy'])
        fetch.assert_not_called()

    def test_restart_rolls_back_source_marker_and_artifact_link(self):
        (self.root/'old-binary').write_bytes(b'previous build')
        (self.root/'panel').symlink_to(self.root/'old-binary')
        transaction=Deployment(self.root)
        transaction.begin(['release.json','new-source.py','panel'])
        atomic_write(self.root/'release.json',json.dumps(self.new))
        (self.root/'new-source.py').write_text('incomplete update')
        (self.root/'panel').unlink();(self.root/'panel').symlink_to(self.root/'new-binary')
        self.updater.save({'busy':True,'status':'installing','last_attempt':100000})
        state=Updater(self.root).check(now=100001)
        self.assertEqual(read(self.root/'release.json'),self.old)
        self.assertFalse((self.root/'new-source.py').exists())
        self.assertEqual((self.root/'panel').read_bytes(),b'previous build')
        self.assertEqual((self.root/'profile.json').read_bytes(),b'{"contrast":17}')
        self.assertFalse(state['busy']);self.assertFalse(transaction.journal.exists())

    def test_failed_reactivation_keeps_journal_for_retry(self):
        (self.root/'build-desktop').mkdir()
        atomic_write(self.root/'build-desktop/install-manifest.json',json.dumps({'release':self.old}))
        transaction=Deployment(self.root);transaction.begin(activation_files());transaction.activation()
        with self.assertRaises(RuntimeError):transaction.rollback(lambda command:(_ for _ in ()).throw(RuntimeError('load failed')))
        self.assertTrue(transaction.journal.exists())
        calls=[];self.assertTrue(transaction.rollback(calls.append));self.assertEqual(len(calls),1)

    def test_git_rollback_keeps_unrelated_untracked_files(self):
        def git(*args):return subprocess.check_output(['git','-C',str(self.root),*args],stderr=subprocess.DEVNULL).decode().strip()
        git('init');git('-c','user.name=Fixture','-c','user.email=fixture@invalid','add','release.json')
        git('-c','user.name=Fixture','-c','user.email=fixture@invalid','commit','-m','old')
        old=git('rev-parse','HEAD');atomic_write(self.root/'release.json',json.dumps(self.new));git('add','release.json')
        git('-c','user.name=Fixture','-c','user.email=fixture@invalid','commit','-m','new')
        new=git('rev-parse','HEAD');git('reset','--hard',old)
        transaction=Deployment(self.root);transaction.begin(['release.json'],old,new)
        git('merge','--ff-only',new);(self.root/'unrelated.txt').write_text('retain me')
        transaction.rollback(lambda command:None)
        self.assertEqual(git('rev-parse','HEAD'),old)
        self.assertEqual(git('diff','--name-only'),'')
        self.assertEqual((self.root/'unrelated.txt').read_text(),'retain me')

    def test_incomplete_candidate_keeps_previous_manifest(self):
        folder=self.root/'build-desktop';folder.mkdir()
        manifest=folder/'install-manifest.json';manifest.write_text('previous build')
        artifacts=self.root/'output/builds/fixture';artifacts.mkdir(parents=True)
        candidate={'root':str(self.root),'artifacts':str(artifacts),'effect_id':'fixture','development_checks':False}
        with self.assertRaisesRegex(RuntimeError,'incomplete'):publish(self.root,candidate)
        self.assertEqual(manifest.read_text(),'previous build')

    def test_failed_activation_restores_previous_artifact_links_and_source(self):
        old=self.root/'old-panel';old.write_text('previous binary')
        panel=self.root/'build/truehdr-panel';panel.parent.mkdir();panel.symlink_to(old)
        artifacts=self.root/'output/builds/fixture';artifacts.mkdir(parents=True)
        for name in ('desktop/truehdr-desktop-session','desktop/desktop_input.spv','desktop/effect.json',
                     'desktop/bootstrap/libtruehdr_bootstrap.so','desktop/bootstrap/qmldir','controls/truehdr-panel',
                     'desktop/kwin/effects/plugins/fixture.so'):
            path=artifacts/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('candidate')
        candidate={'root':str(self.root),'artifacts':str(artifacts),'effect_id':'fixture','development_checks':False,'release':self.new}
        transaction=Deployment(self.root);transaction.begin(['release.json',*activation_files()]);transaction.activation()
        publish(self.root,candidate);atomic_write(self.root/'release.json',json.dumps(self.new))
        self.assertEqual(panel.read_text(),'candidate')
        transaction.rollback(lambda command:None)
        self.assertEqual(panel.read_text(),'previous binary');self.assertEqual(read(self.root/'release.json'),self.old)
        self.assertFalse((self.root/'output/updates/installed-release.json').exists())
        self.assertEqual((self.root/'profile.json').read_bytes(),b'{"contrast":17}')

    def test_artwork_font_vendor_and_source_change_build_identity(self):
        for name in ('CMakeLists.txt','prepare_desktop.py','sdk.py','native/effect.cpp','assets/lightning.png','vendor/fonts/body.ttf','vendor/imgui/imgui.cpp'):
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'original')
        baseline=fingerprint(self.root,self.root,'kwin fixture',False)
        for name in ('native/effect.cpp','assets/lightning.png','vendor/fonts/body.ttf','vendor/imgui/imgui.cpp'):
            path=self.root/name;path.write_bytes(b'changed')
            self.assertNotEqual(baseline,fingerprint(self.root,self.root,'kwin fixture',False),name)
            path.write_bytes(b'original')
        self.assertNotEqual(baseline,fingerprint(self.root,self.root,'kwin fixture',True))
        self.assertNotEqual(baseline,fingerprint(self.root,self.root,'kwin next',False))
        self.assertNotEqual(baseline,fingerprint(self.root,self.root,'kwin fixture',False,'new compiler'))


if __name__=='__main__':unittest.main()
