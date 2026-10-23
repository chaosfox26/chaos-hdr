import contextlib
import io
import json
from pathlib import Path
import runpy
import sys
import tempfile
import unittest
from unittest.mock import patch

import desktop


class VersionGuardTests(unittest.TestCase):
    def setUp(self):
        scratch=desktop.ROOT/'output/scratch'
        scratch.mkdir(parents=True,exist_ok=True)
        self.folder=tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.folder.cleanup)
        self.root=Path(self.folder.name)
        (self.root/'build-desktop').mkdir()
        (self.root/'output/desktop').mkdir(parents=True)
        self.data={'kwin_version':'kwin 6.8.0','root':str(self.root),'effect_id':'truehdr_desktop_fixture'}
        self.save()
        self.addCleanup(patch.stopall)
        patch.object(desktop,'ROOT',self.root).start()
        self.installed=patch.object(desktop.subprocess,'check_output',return_value='kwin 6.8.0\n').start()
        self.call=patch.object(desktop,'call',return_value='KWin version: 6.8.0\n').start()

    def save(self):
        (self.root/'build-desktop/install-manifest.json').write_text(json.dumps(self.data))

    def test_matching_versions_accept_manifest(self):
        self.assertEqual(desktop.manifest(),self.data)

    def test_updated_kwin_blocks_attach_before_any_dbus_call(self):
        self.installed.return_value='kwin 6.8.1\n'
        with self.assertRaisesRegex(RuntimeError,'built for kwin 6.8.0; installed: kwin 6.8.1.*bootstrap.sh.*Alt\\+Z'):
            desktop.attach()
        self.call.assert_not_called()

    def test_running_version_mismatch_blocks_attach_before_loading(self):
        self.call.return_value='KWin version: 6.7.5\n'
        with self.assertRaisesRegex(RuntimeError,'Running KWin: 6.7.5; installed: kwin 6.8.0.*Log out'):
            desktop.attach()
        self.call.assert_called_once_with('/KWin','org.kde.KWin.supportInformation')

    def test_unknown_running_version_fails_closed(self):
        self.call.return_value='Unavailable'
        with self.assertRaisesRegex(RuntimeError,'Running KWin: unknown'):
            desktop.attach()
        self.call.assert_called_once()

    def test_moved_project_requires_rebuild(self):
        self.data['root']=str(self.root/'previous-location')
        self.save()
        with self.assertRaisesRegex(RuntimeError,'project moved.*bootstrap.sh'):
            desktop.attach()
        self.call.assert_not_called()

    def test_attach_command_reports_actionable_error_without_traceback(self):
        self.installed.return_value='kwin 6.8.1\n'
        error=io.StringIO()
        with patch('project.ROOT',self.root),patch.object(sys,'argv',['desktop.py','--attach-only']),contextlib.redirect_stderr(error):
            with self.assertRaises(SystemExit) as caught:
                runpy.run_path(str(Path(__file__).parent/'desktop.py'),run_name='__main__')
        self.assertEqual(caught.exception.code,1)
        self.assertIn('built for kwin 6.8.0; installed: kwin 6.8.1',error.getvalue())
        self.assertIn('bootstrap.sh',error.getvalue())
        self.assertNotIn('Traceback',error.getvalue())
        self.call.assert_not_called()


class UnloadGuardTests(unittest.TestCase):
    xml='<node><interface name="org.truehdr.Desktop"><method name="prepareUnload"/></interface></node>'

    @patch('desktop.time.sleep')
    def test_pending_cleanup_is_polled_before_return(self,sleep):
        with patch('desktop.call',side_effect=[self.xml,'false','false','true']) as call:
            desktop.prepare_unload()
        self.assertEqual(call.call_count,4);self.assertEqual(sleep.call_count,2)

    def test_legacy_effect_keeps_existing_unload_path(self):
        with patch('desktop.call',return_value='<node/>') as call:
            desktop.prepare_unload()
        call.assert_called_once()

    @patch('desktop.time.monotonic',side_effect=[0,3])
    def test_timeout_does_not_unload_the_effect(self,clock):
        def call(path,method,*args):
            if method.endswith('loadedEffects'):return 'truehdr_desktop_fixture'
            if method.endswith('Get'):return 'truehdr_desktop_fixture'
            if method.endswith('Introspect'):return self.xml
            if method.endswith('prepareUnload'):return 'false'
            self.fail('Unexpected unload after timeout: '+method)
        with patch('desktop.call',side_effect=call),self.assertRaisesRegex(RuntimeError,'kept in place'):
            desktop.stop()


if __name__=='__main__':
    unittest.main()
