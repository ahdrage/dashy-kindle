"""Run only the bootstrap's unpacking stage, inside an isolated temporary tree."""
import base64
import io
import lzma
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(
    (ROOT / ".deps/winterbreak2-jb.sh").is_file()
    and (ROOT / "output/unlock/winterbreak2/jb.sh").is_file(),
    "optional upstream payload is absent; follow setup/README.md to prepare it",
)
class UnlockUnpackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        # The PW1 has tar, but its log confirms base64 is absent.
        for name in ("rm", "mkdir", "tar"):
            (self.bin / name).symlink_to(shutil.which(name))
        original = (ROOT / ".deps/winterbreak2-jb.sh").read_text()
        encoded = re.search(r'echo "([A-Za-z0-9+/=]+)"', original.splitlines()[4]).group(1)
        self.payload = lzma.decompress(base64.b64decode(encoded))
        self.archive = self.root / "kmc.tar"
        self.archive.write_bytes(self.payload)

    def unpack(self):
        script = (ROOT / "output/unlock/winterbreak2/jb.sh").read_text()
        prefix = script.split("# Packed from 00_helpers.sh", 1)[0]
        prefix = prefix.replace("/tmp/kmc", str(self.root / "unpacked"))
        prefix = prefix.replace("/mnt/us/winterbreak2/kmc.tar", str(self.archive))
        # This sentinel represents reaching the system-changing installer body.
        prefix += '\nprintf "INSTALLER_BODY_REACHED\\n"\n'
        script_file = self.root / "unpack.sh"
        script_file.write_text(prefix)
        return subprocess.run(["/bin/sh", str(script_file)], env=dict(os.environ, PATH=str(self.bin)),
                              text=True, capture_output=True)

    def test_unpack_on_device_without_base64_preserves_payload(self):
        result = self.unpack()
        self.assertEqual(result.returncode, 0, result.stderr)
        target = self.root / "unpacked/kindlepw2/bin/sh_integration_launcher"
        self.assertTrue(target.is_file(), result.stderr)
        with tarfile.open(fileobj=io.BytesIO(self.payload)) as archive:
            expected = archive.extractfile("./kindlepw2/bin/sh_integration_launcher").read()
        self.assertEqual(target.read_bytes(), expected)
        self.assertIn("INSTALLER_BODY_REACHED", result.stdout)

    def test_bad_archive_stops_before_installer_body(self):
        self.archive.write_bytes(b"broken archive")
        result = self.unpack()
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("INSTALLER_BODY_REACHED", result.stdout)


if __name__ == "__main__":
    unittest.main()
