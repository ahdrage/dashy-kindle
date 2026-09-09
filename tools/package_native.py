"""Package the compiled native demo with its Kindle runtime and startup controls."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile

from build_native import ROOT, OUT, SDK, SDK_COMMIT, RUNTIME_SHA, sha256


def verify_arm_elf(path):
    data = path.read_bytes()
    if len(data) < 52 or data[:7] != b"\x7fELF\x01\x01\x01" or struct.unpack_from("<H", data, 18)[0] != 40:
        raise ValueError("Expected a 32-bit little-endian ARM program")
    table, entry_size, count = struct.unpack_from("<I", data, 28)[0], *struct.unpack_from("<HH", data, 42)
    for i in range(count):
        kind = struct.unpack_from("<I", data, table + i * entry_size)[0]
        if kind in (2, 3):
            raise ValueError("Program must be statically linked, without a dynamic loader")
    if len(data) > 8 * 1024 * 1024:
        raise ValueError("Program exceeds the Kindle runtime's size limit")


def package():
    program = OUT / "dashy.elf"
    verify_arm_elf(program)
    runtime = ROOT / ".deps/kinduino-device-0.5.2.zip"
    if sha256(runtime) != RUNTIME_SHA:
        raise ValueError("Kinduino runtime checksum does not match the pinned release")
    bundle = OUT / "Dashy-PW1-demo"
    staging = OUT / "Dashy-PW1-demo.staging"
    if staging.exists():
        shutil.rmtree(staging)
    usb = staging / "USB-ROOT"
    extension = usb / "extensions"
    extension.mkdir(parents=True)
    with zipfile.ZipFile(runtime) as archive:
        for entry in archive.infolist():
            relative = Path(entry.filename)
            if relative.is_absolute() or ".." in relative.parts or relative.parts[0] != "kinduino":
                raise ValueError("Unexpected path in Kindle runtime archive")
            extracted = Path(archive.extract(entry, extension))
            if not entry.is_dir():
                extracted.chmod((entry.external_attr >> 16) & 0o777 or 0o644)
    shutil.copytree(ROOT / "native/device", extension / "dashy")
    shutil.copytree(ROOT / "native/scriptlets", usb / "documents")
    (usb / "winterbreak2").mkdir()
    shutil.copy2(ROOT / "setup/device/dialoger-launch.html", usb / "winterbreak2/dialoger-launch.html")
    shutil.copy2(ROOT / "native/RUNME.sh", usb / "RUNME.sh")
    (usb / "RUNME.sh").chmod(0o755)
    for script in (extension / "dashy").glob("*.sh"):
        script.chmod(0o755)
    (usb / "dashy").mkdir()
    shutil.copy2(ROOT / "native/browser-launch.sh", usb / "dashy/apply-landscape.sh")
    (usb / "dashy/README.txt").write_text(
        "Dashy starts automatically only after the device test and explicit enable step.\n"
        "To prevent Dashy at boot: create an empty file named DISABLE here, then reboot.\n"
        "You can also remove autostart-enabled if present.\n"
        "Hold the Kindle horizontally with its USB/power edge on the left.\n"
        "While Dashy is running, hold the screen's top-right corner for about 2 seconds to exit.\n"
        "This is the original top-left corner when the Kindle is upright.\n")
    upload = usb / ".kinduino"
    upload.mkdir()
    shutil.copy2(program, upload / "sketch.elf")
    shutil.copytree(OUT / "data", upload / "assets")
    payload = hashlib.sha256(program.read_bytes())
    assets = sorted((upload / "assets").iterdir())
    for asset in assets:
        payload.update(asset.name.encode())
        payload.update(asset.read_bytes())
    build_id = "dashy-demo-" + payload.hexdigest()[:16]
    manifest = ["version: 2", "api_version: 2.0.0", "name: dashy", "target_arch: armv7",
                f"build_id: {build_id}", "elf: sketch.elf", f"sha256: {sha256(program)}",
                f"size: {program.stat().st_size}", f"asset_count: {len(assets)}"]
    manifest += [f"asset: {asset.name} {sha256(asset)} {asset.stat().st_size}" for asset in assets]
    (upload / "manifest").write_text("\n".join(manifest) + "\n")
    (upload / "READY").write_text(build_id + "\n")
    (staging / "START-HERE.md").write_text(
        "# Dashy for Paperwhite 1\n\n"
        "Read [installation and recovery](source/native/INSTALL.md) before copying USB-ROOT.\n\n"
        "The final landscape orientation has the USB/power edge on the left. "
        "Automatic startup is disabled.\n\n"
        "Complete source, build instructions and setup lessons: "
        "https://github.com/ahdrage/dashy-kindle\n")
    shutil.copy2(ROOT / "NOTICE.md", staging / "NOTICE.md")
    shutil.copy2(ROOT / "LICENSE", staging / "LICENSE")
    # Include source for this app, plus notices for statically linked libraries.
    source = staging / "source"
    for directory in ("native", "setup", "tools", "tests", "assets", "web", "docs"):
        shutil.copytree(ROOT / directory, source / directory,
                        ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "node_modules", ".DS_Store"))
    for filename in ("README.md", "DEVICE-SETUP.md", "NOTICE.md", "LICENSE", "dashy.py",
                     "requirements.txt", "requirements-dev.txt", ".gitignore"):
        shutil.copy2(ROOT / filename, source / filename)
    shutil.copytree(OUT / "sketch", source / "compiled-sketch")
    shutil.copy2(ROOT / "sample-data.json", source / "sample-data.json")
    licenses = staging / "licenses"
    licenses.mkdir()
    shutil.copy2(SDK / "LICENSE", licenses / "Kinduino-MIT.txt")
    zig = ROOT / ".deps/zig-aarch64-macos-0.16.0"
    for name, path in (("musl.txt", "lib/libc/musl/COPYRIGHT"), ("libcxx.txt", "lib/libcxx/LICENSE.TXT"),
                       ("libcxxabi.txt", "lib/libcxxabi/LICENSE.TXT"), ("libunwind.txt", "lib/libunwind/LICENSE.TXT"),
                       ("zig.txt", "LICENSE")):
        shutil.copy2(zig / path, licenses / name)
    # Vendored files retain their full copyright and license notices.
    shutil.copy2(SDK / "arduino/libraries/GFX/Adafruit_GFX.cpp", licenses / "Adafruit_GFX-source-and-license.cpp")
    stb = (SDK / "arduino/libraries/Font/stb_truetype.h").read_text()
    (licenses / "stb_truetype.txt").write_text(stb[stb.index("This software is available under 2 licenses"):])
    shutil.copy2(ROOT / "assets/fonts/OFL.txt", licenses / "Montserrat-OFL.txt")
    (staging / "build-info.json").write_text(json.dumps({
        "build_id": build_id, "device": "Kindle Paperwhite 1 Wi-Fi, B024, firmware 5.6.1.1",
        "sdk_commit": SDK_COMMIT, "runtime": "Kinduino 0.5.2", "runtime_sha256": RUNTIME_SHA,
        "compiler": "Zig 0.16.0", "board": "pw1_storage", "api_version": "2.0.0",
        "program_sha256": sha256(program), "program_size": program.stat().st_size,
        "orientation": "landscape", "logical_size": [1024, 758], "usb_power_edge": "left",
        "physical_device_tested": False, "autostart_enabled": False,
    }, indent=2) + "\n")
    checksums = [f"{sha256(path)}  {path.relative_to(staging).as_posix()}"
                 for path in sorted(staging.rglob("*")) if path.is_file()]
    (staging / "SHA256SUMS").write_text("\n".join(checksums) + "\n")
    if bundle.exists():
        shutil.rmtree(bundle)
    staging.rename(bundle)
    archive = shutil.make_archive(str(bundle), "zip", OUT, bundle.name)
    print(f"Install bundle: {archive}")
    print(f"SHA-256: {sha256(Path(archive))}")
    return bundle

if __name__ == "__main__":
    package()
