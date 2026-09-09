"""Install the finished ARM bundle into a temporary directory using the real runtime's C code."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "output/native"
BUNDLE = OUT / "Dashy-PW1-demo/USB-ROOT"


class BundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sdk = ROOT / ".deps/kinduino/device"
        watch = sdk / "watcher/src"
        launcher = sdk / "launcher/src"
        sources = [watch / name for name in ("install_slot.c", "validate.c", "manifest.c", "sha256.c", "sha256_file.c", "asset_install.c")]
        sources += [launcher / "gen_menu.c", launcher / "elf_check.c"]
        subprocess.run(["cc", "-O2", "-Wall", "-Wextra", f"-I{watch}", f"-I{launcher}",
                        str(ROOT / "native/tests/install_bundle.c"), *map(str, sources),
                        "-o", str(OUT / "install-bundle-test")], check=True)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        # Use a canonical path in diagnostics and in the isolated installer fixture.
        self.root = Path(self.tmp.name).resolve()
        self.upload = self.root / "upload"
        self.runtime = self.root / "runtime"
        self.runtime.mkdir()
        self.assertTrue((BUNDLE / ".kinduino").is_dir(), "The installable Dashy bundle has not been created")
        shutil.copytree(BUNDLE / ".kinduino", self.upload)

    def install(self):
        return subprocess.run([str(OUT / "install-bundle-test"), str(self.upload),
                               str(self.runtime), str(self.root / "menu.json")], capture_output=True, text=True)

    def test_complete_bundle_is_accepted_and_installs_executable_and_fonts(self):
        result = self.install()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        slot = self.runtime / "sketches/dashy"
        self.assertTrue(os.access(slot / "sketch.elf", os.X_OK))
        self.assertEqual((slot / "sketch.elf").read_bytes(), (OUT / "dashy.elf").read_bytes())
        self.assertEqual(len(list((slot / "assets").glob("*.ttf"))), 2)
        self.assertFalse((self.upload / "READY").exists())
        self.assertIn("dashy", json.dumps(json.loads((self.root / "menu.json").read_text())))
        self.assertFalse((BUNDLE / "dashy/autostart-enabled").exists())

    def test_corrupted_program_is_rejected_without_installing(self):
        program = self.upload / "sketch.elf"
        data = bytearray(program.read_bytes())
        data[-1] ^= 1
        program.write_bytes(data)
        result = self.install()
        self.assertEqual(result.returncode, 4, result.stdout + result.stderr)
        self.assertTrue((self.upload / "READY").exists())
        self.assertFalse((self.runtime / "sketches/dashy/sketch.elf").exists())

    def test_incomplete_font_payload_is_rejected_without_installing(self):
        (self.upload / "assets/DashySans-Regular.ttf").unlink()
        result = self.install()
        self.assertEqual(result.returncode, 6, result.stdout + result.stderr)
        self.assertTrue((self.upload / "READY").exists())
        self.assertFalse((self.runtime / "sketches/dashy/sketch.elf").exists())


if __name__ == "__main__":
    unittest.main()
