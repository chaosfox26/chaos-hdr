import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import urllib.error

import updates


def release(tag='v0.3.0',prerelease=False):
    return {'tag_name':tag,'prerelease':prerelease,'draft':False,'assets':[
        {'name':updates.INSTALLER,'browser_download_url':updates.DOWNLOAD+tag+'/'+updates.INSTALLER,
         'digest':'sha256:'+hashlib.sha256(b'installer fixture').hexdigest()}]}


class UpdateTests(unittest.TestCase):
    def setUp(self):
        scratch=updates.ROOT/'output/scratch';scratch.mkdir(parents=True,exist_ok=True)
        self.folder=tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.folder.cleanup)
        self.root=Path(self.folder.name)
        (self.root/'release.json').write_text(json.dumps({'tag':'v0.2.0','prerelease':True}))
        (self.root/'profile.json').write_text('{"enabled":true}\n')
        self.updater=updates.Updater(self.root)
        self.source=self.root/'staged-source';self.source.mkdir()
        (self.source/'release.json').write_text(json.dumps({'tag':'v0.3.0','prerelease':False}))
        staged=patch.object(self.updater,'stage_payload',return_value=(self.source,['release.json']))
        staged.start();self.addCleanup(staged.stop)
        publisher=patch('prepare_desktop.publish');publisher.start();self.addCleanup(publisher.stop)
        self.candidate=self.updater.folder/'candidate.json'
        self.candidate.write_text(json.dumps({'release':{'tag':'v0.2.0','prerelease':True},'development_checks':False}))

    def complete_release(self,*args):
        self.candidate.write_text(json.dumps({'release':{'tag':'v0.3.0','prerelease':False},'development_checks':False}))

    def test_semantic_version_and_prerelease_order(self):
        values=['v0.2.0-rc.2','v0.2.0-rc.10','v0.2.0','v0.10.0','v1.0.0']
        self.assertEqual(sorted(values,key=updates.version),values)
        self.assertLess(updates.version('v0.2.0',True),updates.version('v0.2.0'))
        for tag in ('latest','v01.2.0','v1.0.0-01','../v1.0.0','v1.0.0-'):
            with self.assertRaises(ValueError):updates.version(tag)

    def test_stable_channel_ignores_prereleases_and_drafts(self):
        draft=release('v2.0.0');draft['draft']=True
        chosen=updates.choose([release('v0.4.0',True),draft,release('v0.3.0')],{'tag':'v0.2.0','prerelease':False})
        self.assertEqual(chosen['tag_name'],'v0.3.0')

    def test_experimental_channel_accepts_prereleases(self):
        self.assertEqual(updates.choose([release('v0.3.0-rc.1',True)],self.updater.current)['tag_name'],'v0.3.0-rc.1')
        self.assertIsNone(updates.choose([release('v0.1.0',True)],self.updater.current))

    @patch('updates.notify',return_value=True)
    @patch('updates.published_releases',return_value=[release()])
    def test_daily_limit_survives_restart_and_notifies_once(self,fetch,notify):
        result=self.updater.check(now=100000)
        self.assertEqual(result['status'],'available')
        updates.Updater(self.root).check(now=100001)
        self.assertEqual(fetch.call_count,1)
        updates.Updater(self.root).check(now=100000+updates.DAY)
        self.assertEqual(fetch.call_count,2)
        notify.assert_called_once()
        self.assertEqual(self.updater.path.stat().st_mode&0o777,0o600)

    @patch('updates.published_releases',side_effect=urllib.error.URLError('offline'))
    def test_failed_attempt_is_also_limited_and_manual_can_retry(self,fetch):
        result=self.updater.check(now=100000)
        self.assertEqual(result['status'],'error')
        self.assertIn('connection',result['error'])
        self.updater.check(now=100002)
        self.assertEqual(fetch.call_count,1)
        self.updater.check(manual=True,now=100003)
        self.assertEqual(fetch.call_count,2)

    @patch('updates.published_releases',side_effect=urllib.error.HTTPError('url',403,'rate limit',{},None))
    def test_rate_limit_has_useful_status(self,fetch):
        self.assertIn('limited',self.updater.check(now=100000)['error'])

    @patch('updates.published_releases',return_value=[])
    def test_no_published_release(self,fetch):
        self.assertEqual(self.updater.check(now=100000)['status'],'unpublished')

    @patch('updates.published_releases',return_value=[release('v0.1.0',True)])
    def test_older_release_does_not_offer_downgrade(self,fetch):
        self.assertIsNone(self.updater.check(now=100000)['available'])

    def test_wrong_repository_and_missing_digest_refused(self):
        data=release();data['assets'][0]['browser_download_url']='https://example.org/setup'
        with self.assertRaises(ValueError):updates.installer(data)
        data=release();data['assets'][0].pop('digest')
        with self.assertRaises(ValueError):updates.installer(data)

    @patch('updates.published_releases',return_value=[release()])
    def test_locked_operation_does_not_start_another_check(self,fetch):
        with self.updater.lock():
            self.updater.check(manual=True,now=100000)
        fetch.assert_not_called()

    @patch('updates.notify',return_value=True)
    def test_rebuild_uses_existing_setup_and_preserves_profile(self,notify):
        before=(self.root/'profile.json').read_bytes()
        with patch.object(self.updater,'run_setup') as setup,patch('updates.published_releases') as fetch:
            self.assertEqual(self.updater.install(rebuild=True),0)
        self.assertEqual(setup.call_count,2)
        self.assertEqual(setup.call_args.args[0],[str(self.root/'.venv/bin/python'),str(self.root/'install.py')])
        fetch.assert_not_called()
        self.assertEqual((self.root/'profile.json').read_bytes(),before)

    @patch('updates.notify',return_value=True)
    def test_dirty_source_tree_refuses_install_without_downloading(self,notify):
        (self.root/'.git').mkdir()
        with patch.object(self.updater,'git',return_value=' M desktop.py'),patch('updates.published_releases') as fetch:
            self.assertEqual(self.updater.install(),1)
        fetch.assert_not_called()
        self.assertIn('Local source changes',updates.read(self.updater.path)['error'])

    @patch('updates.notify',return_value=True)
    def test_diverged_source_commits_are_not_replaced(self,notify):
        (self.root/'.git').mkdir()
        def git(*args):
            if args[0]=='remote':return 'https://github.com/'+updates.REPOSITORY+'.git'
            if args[0]=='rev-parse':return 'a'*40
            if args[0]=='merge-base':raise subprocess.CalledProcessError(1,args)
            return ''
        with patch.object(self.updater,'git',side_effect=git) as calls,patch('updates.published_releases',return_value=[release()]),patch.object(self.updater,'download',return_value=self.root/'fixture'),patch.object(self.updater,'run_setup') as setup:
            self.assertEqual(self.updater.install(),1)
        self.assertNotIn('merge',[c.args[0] for c in calls.call_args_list])
        setup.assert_not_called()
        self.assertIn('local commits',updates.read(self.updater.path)['error'])

    def test_download_checks_digest_before_setup(self):
        class Response(io.BytesIO):
            url='https://release-assets.githubusercontent.com/fixture'
        with patch('updates.urllib.request.urlopen',return_value=Response(b'corrupt download')):
            with self.assertRaisesRegex(RuntimeError,'verification failed'):
                self.updater.download(release(),self.root)
        with patch('updates.urllib.request.urlopen',return_value=Response(b'installer fixture')):
            path=self.updater.download(release(),self.root)
        self.assertEqual(path.read_bytes(),b'installer fixture')

    @patch('updates.notify',return_value=True)
    def test_packaged_install_uses_verified_payload_and_backup(self,notify):
        payload=self.root/'verified-installer'
        with patch('updates.published_releases',return_value=[release()]),patch.object(self.updater,'download',return_value=payload) as download,patch.object(self.updater,'run_setup',side_effect=self.complete_release) as setup:
            self.assertEqual(self.updater.install(),0)
        download.assert_called_once()
        self.assertEqual(setup.call_count,2)
        backup=next(self.updater.folder.glob('rollback-*'))
        self.assertTrue((backup/'release.json').is_file())
        self.assertEqual((self.root/'profile.json').read_text(),'{"enabled":true}\n')

    @patch('updates.notify',return_value=True)
    def test_build_failure_clears_busy_and_offers_repair(self,notify):
        with patch.object(self.updater,'run_setup',side_effect=RuntimeError('Setup failed. Try Rebuild.')):
            self.assertEqual(self.updater.install(rebuild=True),1)
        state=updates.read(self.updater.path)
        self.assertFalse(state['busy'])
        self.assertEqual(state['status'],'error')
        self.assertIn('Rebuild',state['error'])

    @patch('updates.published_releases')
    def test_source_version_refresh_does_not_trigger_extra_automatic_request(self,fetch):
        updates.atomic_write(self.updater.path,json.dumps({'current_tag':'v0.1.0','last_attempt':100000,'status':'available','available':{'tag':'v0.2.0','prerelease':True}}))
        state=self.updater.check(now=100001)
        self.assertIsNone(state['available'])
        self.assertEqual(state['current_tag'],'v0.2.0')
        fetch.assert_not_called()

    @patch('updates.notify',return_value=True)
    def test_clean_source_update_only_fast_forwards(self,notify):
        (self.root/'.git').mkdir()
        def git(*args):
            if args[0]=='remote':return 'https://github.com/'+updates.REPOSITORY+'.git'
            if args[0]=='rev-parse':return 'a'*40
            return ''
        with patch.object(self.updater,'git',side_effect=git) as calls,patch('updates.published_releases',return_value=[release()]),patch.object(self.updater,'download',return_value=self.root/'fixture'),patch.object(self.updater,'run_setup',side_effect=self.complete_release) as setup,patch('updates.subprocess.check_output',return_value=(self.source/'release.json').read_bytes()):
            self.assertEqual(self.updater.install(),0)
        self.assertIn(unittest.mock.call('merge','--ff-only','a'*40),calls.call_args_list)
        self.assertEqual(setup.call_count,2)
        self.assertEqual(setup.call_args.args[0],[str(self.root/'.venv/bin/python'),str(self.root/'install.py')])

    def test_source_and_cmake_release_versions_match(self):
        root=Path(__file__).parent
        tag=json.loads((root/'release.json').read_text())['tag'].removeprefix('v')
        for file in ('CMakeLists.txt','native/desktop/CMakeLists.txt'):
            self.assertIn('VERSION '+tag,(root/file).read_text())

    @patch('updates.notify',return_value=True)
    def test_rebuild_keeps_newer_release_available(self,notify):
        self.updater.save({'available':{'tag':'v0.3.0','prerelease':False}})
        with patch.object(self.updater,'run_setup'):
            self.assertEqual(self.updater.install(rebuild=True),0)
        self.assertEqual(updates.read(self.updater.path)['available']['tag'],'v0.3.0')

    @patch('updates.notify',return_value=True)
    def test_successful_setup_with_wrong_version_is_not_success(self,notify):
        with patch('updates.published_releases',return_value=[release()]),patch.object(self.updater,'download',return_value=self.root/'fixture'),patch.object(self.updater,'run_setup'):
            self.assertEqual(self.updater.install(),1)
        self.assertIn('requested release version',updates.read(self.updater.path)['error'])

    @patch('updates.notify',return_value=True)
    def test_offline_install_reports_connection_error(self,notify):
        with patch('updates.published_releases',side_effect=urllib.error.URLError('offline')):
            self.assertEqual(self.updater.install(),1)
        self.assertIn('connection',updates.read(self.updater.path)['error'])

    def test_different_source_origin_is_refused(self):
        (self.root/'.git').mkdir()
        with patch.object(self.updater,'git',side_effect=['','https://example.org/other-project']):
            with self.assertRaisesRegex(RuntimeError,'different origin'):
                self.updater.source_preflight()

    @patch('updates.published_releases',return_value=[])
    def test_invalid_status_data_recovers_with_a_check(self,fetch):
        self.updater.path.write_text('[]')
        self.assertEqual(self.updater.check(now=100000)['status'],'unpublished')

    def test_uninstall_stops_transient_update_units(self):
        import install
        units=self.root/'units';units.mkdir()
        def result(args,**kwargs):return subprocess.CompletedProcess(args,3 if 'is-active' in args else 0)
        with patch.multiple(install,ROOT=self.root,UNITS=units,DESKTOP=self.root/'desktop-entry',UNINSTALL_COMMAND=self.root/'uninstall-link',SETTINGS=self.root/'settings'),patch.object(install.subprocess,'run',side_effect=result) as calls,patch('desktop.stop'),patch.object(Path,'home',return_value=self.root):
            install.uninstall()
        for name in ('chaos-hdr-update','chaos-hdr-release-check'):
            self.assertIn(unittest.mock.call(['systemctl','--user','stop',name],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL),calls.call_args_list)


if __name__=='__main__':
    unittest.main()
