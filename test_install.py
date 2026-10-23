import contextlib
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import install


class InstallTests(unittest.TestCase):
    def test_services_start_with_resolvable_desktop_identity_and_existing_profiles(self):
        scratch = install.ROOT / 'output/scratch'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            root = Path(directory)
            desktop = root / 'applications/truehdr-linux.desktop'
            units = root / 'units'
            profiles = {
                'profile.json': '{"enabled":true,"contrast":17}\n',
                'profiles.json': '{"saved":"fixture"}\n',
                'window-targets.json': '{"targets":[]}\n',
            }
            for name, content in profiles.items():
                (root / name).write_text(content)
            for name in ('.venv/bin/python', 'build/truehdr-panel', 'uninstall.sh',
                         'build-desktop/truehdr-desktop-session',
                         'build-desktop/bootstrap/libtruehdr_bootstrap.so',
                         'build-desktop/kwin/effects/plugins/fixture-effect.so'):
                file = root / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.touch()
            starts = []

            def systemctl(args, **kwargs):
                self.assertEqual(args[:2], ['systemctl', '--user'])
                if 'enable' in args and '--now' in args:
                    self.assertTrue(desktop.is_file())
                    self.assertIn(f'Exec="{root}/build/truehdr-panel" "{root}/profile.json"',
                                  desktop.read_text())
                    for name, content in profiles.items():
                        self.assertEqual((root / name).read_text(), content)
                    starts.append(args)
                return subprocess.CompletedProcess(args, 0)

            with patch.multiple(install, ROOT=root, UNITS=units, DESKTOP=desktop,
                                UNINSTALL_COMMAND=root / 'bin/uninstall',
                                SETTINGS=root / 'launch-settings.json'), \
                    patch.object(install.subprocess, 'run', side_effect=systemctl), \
                    patch('desktop.manifest', return_value={'effect_id': 'fixture-effect','release':{'tag':'v0.3.0','prerelease':True}}), \
                    patch('desktop.start'), patch('desktop.refresh_display'), \
                    patch.object(Path, 'home', return_value=root), \
                    contextlib.redirect_stdout(io.StringIO()):
                install.install()
            self.assertEqual(len(starts), 1)
            self.assertIn('v0.3.0',(root/'output/updates/installed-release.json').read_text())


if __name__ == '__main__':
    unittest.main()
