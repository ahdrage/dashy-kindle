"""Exercise device startup decisions against a temporary filesystem and inert hardware tools."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import sys

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "native/device"


class DeviceControlsTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        for name in ("proc", "var/run", "etc/upstart", "var/local/dashy", "var/local/kinduino/bin",
                     "var/local/kinduino/sketches/dashy", "mnt/us/system", "mnt/us/dashy",
                     "mnt/us/extensions/dashy", "mnt/us/extensions/kinduino/bin", "mnt/us/.kinduino", "sbin", "test-bin"):
            (self.root / name).mkdir(parents=True, exist_ok=True)
        (self.root / "proc/usid").write_text("B024-test-only\n")
        (self.root / "mnt/us/system/version.txt").write_text("Kindle 5.6.1.1 (268989 035)\n")
        (self.root / "mnt/us/extensions/kinduino/.installed").write_text("version=0.5.2\napi=2.0.0\n")
        (self.root / "var/local/kinduino/sketches/dashy/sketch.elf").write_bytes(b"dummy")
        (self.root / "mnt/us/.kinduino/READY").write_text("test")
        for script in SCRIPTS.iterdir():
            if script.is_file():
                shutil.copy2(script, self.root / "mnt/us/extensions/dashy" / script.name)
                shutil.copy2(script, self.root / "var/local/dashy" / script.name)
        self.tool("test-bin/id", "echo 0")
        self.tool("test-bin/uname", "echo armv7l")
        self.tool("test-bin/mntroot", 'echo "mntroot $*" >> "$DASHY_ROOT/actions"')
        self.tool("sbin/initctl", 'echo "initctl $*" >> "$DASHY_ROOT/actions"')
        self.tool("var/local/kinduino/bin/kinduino-supervisor", 'echo "supervisor $*" >> "$DASHY_ROOT/actions"; exit 1')
        self.tool("var/local/kinduino/bin/kinduino-watch", 'echo "install $*" >> "$DASHY_ROOT/actions"')
        self.tool("mnt/us/extensions/kinduino/bin/kinduino-install", 'echo "runtime-install" >> "$DASHY_ROOT/actions"')
        self.tool("var/local/kinduino/bin/kinduino-detach", 'echo "detach $*" >> "$DASHY_ROOT/actions"')
        self.env = dict(os.environ, DASHY_ROOT=str(self.root), PATH=str(self.root / "test-bin") + os.pathsep + os.environ["PATH"])

    def tool(self, path, body):
        target = self.root / path
        target.write_text("#!/bin/sh\n" + body + "\n")
        target.chmod(0o755)

    def run_script(self, script, *args):
        return subprocess.run(["sh", str(SCRIPTS / script), *args], env=self.env, capture_output=True, text=True)

    def actions(self):
        path = self.root / "actions"
        return path.read_text() if path.exists() else ""

    def enable_flag(self):
        (self.root / "mnt/us/dashy/autostart-enabled").touch()

    def test_no_launch_without_enabled_flag_or_with_usb_disable_file(self):
        self.assertEqual(self.run_script("boot.sh").returncode, 0)
        self.enable_flag()
        (self.root / "mnt/us/dashy/DISABLE").touch()
        self.assertEqual(self.run_script("boot.sh").returncode, 0)
        self.assertEqual(self.actions(), "")

    def test_failed_launch_is_attempted_only_once_per_boot(self):
        self.enable_flag()
        self.assertNotEqual(self.run_script("boot.sh").returncode, 0)
        self.assertEqual(self.run_script("boot.sh").returncode, 0)
        self.assertEqual(self.actions().count("supervisor dashy"), 1)

    def test_wrong_device_and_firmware_do_not_change_startup(self):
        for bad_path, bad_value in (("proc/usid", "B0D4-other-device"),
                                    ("mnt/us/system/version.txt", "Kindle 5.16.0")):
            path = self.root / bad_path
            original = path.read_text()
            path.write_text(bad_value)
            result = self.run_script("control.sh", "enable", "--tested")
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(self.actions(), "")
            self.assertFalse((self.root / "etc/upstart/dashy.conf").exists())
            path.write_text(original)

    def test_enable_requires_physical_test_flag_and_preserves_other_job(self):
        self.assertNotEqual(self.run_script("control.sh", "enable").returncode, 0)
        self.assertEqual(self.actions(), "")
        job = self.root / "etc/upstart/dashy.conf"
        job.write_text("unrelated user job\n")
        self.assertNotEqual(self.run_script("control.sh", "enable", "--tested").returncode, 0)
        self.assertEqual(job.read_text(), "unrelated user job\n")
        self.assertEqual(self.actions(), "")

    def test_enable_then_disable_keeps_startup_opt_in_and_stops_job(self):
        result = self.run_script("control.sh", "enable", "--tested")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.root / "mnt/us/dashy/autostart-enabled").exists())
        job = (self.root / "etc/upstart/dashy.conf").read_text()
        self.assertIn("start on started framework", job)
        self.assertNotIn("stop on stopped framework", job)
        self.assertNotIn("respawn", job)
        self.assertIn("mntroot rw", self.actions())
        self.assertIn("mntroot ro", self.actions())
        result = self.run_script("control.sh", "disable")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.root / "mnt/us/dashy/DISABLE").exists())
        self.assertFalse((self.root / "mnt/us/dashy/autostart-enabled").exists())
        self.assertIn("initctl stop dashy", self.actions())

    def test_install_delegates_bundle_validation_without_enabling_startup(self):
        result = self.run_script("control.sh", "install")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("install --install-slot", self.actions())
        self.assertFalse((self.root / "mnt/us/dashy/autostart-enabled").exists())
        self.assertFalse((self.root / "etc/upstart/dashy.conf").exists())
        self.assertNotIn("mntroot", self.actions())

    def test_failed_startup_copy_leaves_override_and_remounts_read_only(self):
        self.tool("test-bin/cp", "exit 1")
        result = self.run_script("control.sh", "enable", "--tested")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((self.root / "mnt/us/dashy/DISABLE").exists())
        self.assertFalse((self.root / "mnt/us/dashy/autostart-enabled").exists())
        self.assertFalse((self.root / "etc/upstart/dashy.conf").exists())
        self.assertIn("mntroot ro", self.actions())

    def test_non_kindle_host_is_rejected_before_any_action(self):
        self.tool("test-bin/uname", "echo arm64")
        self.assertNotEqual(self.run_script("control.sh", "install").returncode, 0)
        self.assertEqual(self.actions(), "")

    def test_library_installer_installs_runtime_then_app_without_starting_it(self):
        script = ROOT / "native/scriptlets/Install Dashy.sh"
        result = subprocess.run(["sh", str(script)], env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertLess(self.actions().index("runtime-install"), self.actions().index("install --install-slot"))
        self.assertNotIn("detach", self.actions())
        self.assertFalse((self.root / "mnt/us/dashy/autostart-enabled").exists())

    def test_library_launcher_detaches_supervisor_to_survive_reader_shutdown(self):
        script = ROOT / "native/scriptlets/Open Dashy.sh"
        result = subprocess.run(["sh", str(script)], env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("detach " + str(self.root / "var/local/kinduino/bin/kinduino-supervisor") + " dashy", self.actions())

    def current_session(self, name="dashy"):
        marker = self.root / "var/local/kinduino/run/current"
        marker.parent.mkdir(parents=True, exist_ok=True)
        marker.write_text(name + "\n")
        return marker

    def session_process(self, mode="supervisor", ignores_term=False):
        # A real disposable process handles TERM; only its proc metadata is simulated.
        worker = r'''
import os, pathlib, shutil, signal, sys, time
root = pathlib.Path(sys.argv[1]); mode = sys.argv[2]
proc = root / "proc" / str(os.getpid()); proc.mkdir()
runtime = root / "var/local/kinduino"
args = ["/bin/sh", str(runtime / "bin/kinduino-supervisor"), "dashy"] if mode == "supervisor" else [str(runtime / "sketches/dashy/sketch.elf")]
(proc / "cmdline").write_bytes(b"\0".join(s.encode() for s in args) + b"\0")
(proc / "status").write_text("PPid:\t1\n")
def stop(signum, frame):
    time.sleep(0.15)
    (root / "stopped-cleanly").write_text(str(signum))
    (runtime / "run/current").unlink(missing_ok=True)
    shutil.rmtree(proc)
    sys.exit(0)
signal.signal(signal.SIGTERM, signal.SIG_IGN if sys.argv[3] == "1" else stop)
print("READY", flush=True)
while True: time.sleep(0.1)
'''
        process = subprocess.Popen([sys.executable, "-c", worker, str(self.root), mode,
                                    "1" if ignores_term else "0"], stdout=subprocess.PIPE, text=True)
        def cleanup():
            if process.poll() is None: process.kill()
            process.wait()
            process.stdout.close()
        self.addCleanup(cleanup)
        self.assertEqual(process.stdout.readline().strip(), "READY")
        return process

    def test_stop_clears_only_an_inactive_dashy_marker(self):
        marker = self.current_session()
        result = self.run_script("stop.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(marker.exists())

    def test_stop_waits_for_existing_supervisor_to_clean_up(self):
        marker = self.current_session()
        process = self.session_process()
        result = self.run_script("stop.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.root / "stopped-cleanly").exists())
        self.assertFalse(marker.exists())
        self.assertEqual(process.wait(timeout=2), 0)

    def test_stop_preserves_session_if_supervisor_does_not_exit(self):
        marker = self.current_session()
        process = self.session_process(ignores_term=True)
        self.env["DASHY_STOP_TIMEOUT"] = "0"
        self.assertNotEqual(self.run_script("stop.sh").returncode, 0)
        self.assertTrue(marker.exists())
        self.assertIsNone(process.poll())

    def test_stop_does_not_clear_marker_over_an_orphaned_app(self):
        marker = self.current_session()
        process = self.session_process(mode="orphan")
        self.env["DASHY_STOP_TIMEOUT"] = "0"
        self.assertNotEqual(self.run_script("stop.sh").returncode, 0)
        self.assertTrue(marker.exists())
        self.assertIsNone(process.poll())

    def test_stop_preserves_another_kinduno_app(self):
        marker = self.current_session("another_app")
        self.assertNotEqual(self.run_script("stop.sh").returncode, 0)
        self.assertEqual(marker.read_text(), "another_app\n")


if __name__ == "__main__":
    unittest.main()
