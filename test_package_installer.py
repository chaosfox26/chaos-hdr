import contextlib
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import package_installer as package


class PackageTests(unittest.TestCase):
    def setUp(self):
        scratch=package.ROOT/'output/scratch';scratch.mkdir(parents=True,exist_ok=True)
        self.folder=tempfile.TemporaryDirectory(dir=scratch);self.addCleanup(self.folder.cleanup)
        self.root=Path(self.folder.name)
        for name,data in {'README.md':b'Fixture source','LICENSE':b'Fixture license','release.json':b'{"tag":"v0.3.0","prerelease":true}',
                          'profile.json':b'{"private":"do not package"}','capture.png':b'private pixels',
                          'github-install.sh.in':b'@INSTALLER_URL@\n@INSTALLER_SHA256@\n'}.items():
            (self.root/name).write_bytes(data)
        self.scope=patch.multiple(package,ROOT=self.root,SOURCE_FILES=('README.md','release.json','github-install.sh.in'))
        self.scope.start();self.addCleanup(self.scope.stop)

    def build(self):
        with contextlib.redirect_stdout(io.StringIO()):return package.build()

    def test_public_payload_is_allowlisted_licensed_and_hash_verified(self):
        installer=self.build();payload=installer.read_bytes().split(b'__TRUEHDR_ARCHIVE__\n',1)[1]
        with tarfile.open(fileobj=io.BytesIO(payload),mode='r:gz') as archive:
            names=archive.getnames();self.assertIn('LICENSE',names);self.assertNotIn('profile.json',names);self.assertNotIn('capture.png',names)
            manifest=json.load(archive.extractfile('INSTALL-MANIFEST.json'));self.assertFalse(manifest['personal_profiles'])
            for name,digest in manifest['files'].items():self.assertEqual(hashlib.sha256(archive.extractfile(name).read()).hexdigest(),digest)

    def test_identical_input_produces_identical_installer(self):
        first=self.build().read_bytes();second=self.build().read_bytes();self.assertEqual(first,second)

    def test_missing_license_fails_without_an_installer(self):
        (self.root/'LICENSE').unlink()
        with self.assertRaisesRegex(RuntimeError,'license'):self.build()
        self.assertFalse((self.root/'output/installers').exists())

    def test_launcher_embeds_only_the_requested_asset_and_actual_digest(self):
        url='https://github.com/fixture/fixture/releases/download/v0.3.0/Chaos-HDR-CachyOS.run'
        with contextlib.redirect_stdout(io.StringIO()):installer=package.build(installer_url=url)
        launcher=(installer.parent/'install.sh').read_text()
        self.assertIn(url,launcher);self.assertIn(hashlib.sha256(installer.read_bytes()).hexdigest(),launcher)


if __name__=='__main__':unittest.main()
