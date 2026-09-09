"""Build the Paperwhite 1 program using the pinned Kinduino SDK and Zig compiler."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from dashy import readings_from_netatmo

SDK_COMMIT = "f77ef002faf1cae520db4038d9c4ee4a3c01fef4"
RUNTIME_SHA = "3cd7e4ef1665e38b626616e4764eb8e6f6a4486a351a629cdae1c401df2c008d"
OUT = ROOT / "output/native"
SDK = ROOT / ".deps/kinduino"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare():
    sketch = OUT / "sketch"
    sketch.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / "native/dashy").iterdir():
        if source.is_file():
            shutil.copy2(source, sketch / source.name)
    data = OUT / "data"
    data.mkdir(parents=True, exist_ok=True)
    for style, weight in (("Regular", 400), ("Medium", 500)):
        font = instantiateVariableFont(TTFont(ROOT / "assets/fonts/Montserrat.ttf"), {"wght": weight})
        # Static instances for stb_truetype; rename the derivative family and retain OFL.
        names = {1: "Dashy Sans", 2: style, 3: f"Dashy Sans {style}", 4: f"Dashy Sans {style}",
                 6: f"DashySans-{style}", 16: "Dashy Sans", 17: style}
        for record in font["name"].names:
            if record.nameID in names:
                record.string = names[record.nameID].encode(record.getEncoding())
        font.save(data / f"DashySans-{style}.ttf")
    shutil.copy2(ROOT / "assets/fonts/OFL.txt", data / "OFL.txt")
    certificate = ROOT / "assets/certs/DigiCertGlobalRootG2.crt"
    import ssl
    if hashlib.sha256(ssl.PEM_cert_to_DER_cert(certificate.read_text())).hexdigest() != "cb3ccbb76031e5e0138f8dd39a23f9de47ffc35e43c1144cea27d46a5ab1cb5f":
        raise ValueError("Unexpected Netatmo root certificate")
    shutil.copy2(certificate, data / certificate.name)
    readings = readings_from_netatmo(json.loads((ROOT / "sample-data.json").read_text()))
    def temperature(value):
        return "--.-°C" if value is None else f"{value:.1f}°C"
    labels = [temperature(readings["indoor_c"]),
              "---- ppm" if readings["co2_ppm"] is None else f'{readings["co2_ppm"]} ppm',
              temperature(readings["outdoor_c"])]
    (sketch / "DemoReadings.h").write_text(
        '#pragma once\n#include "Dashboard.h"\nnamespace dashy {\n'
        'static const Readings demoReadings = {'
        + ", ".join(json.dumps(label, ensure_ascii=False) for label in labels) + '};\n}\n')
    return sketch


def build():
    commit = subprocess.check_output(["git", "-C", str(SDK), "rev-parse", "HEAD"], text=True).strip()
    if commit != SDK_COMMIT:
        raise RuntimeError(f"Expected Kinduino {SDK_COMMIT}; found {commit}")
    zig = ROOT / ".deps/zig-aarch64-macos-0.16.0"
    if not (zig / "zig").is_file():
        raise RuntimeError("Install the pinned compiler first; see native/README.md")
    sketch = prepare()
    env = dict(os.environ, PATH=str(zig) + os.pathsep + os.environ["PATH"],
               ZIG_GLOBAL_CACHE_DIR=str(ROOT / ".deps/zig-cache"),
               ZIG_LOCAL_CACHE_DIR=str(OUT / "zig-cache"))
    # Compiler-library diagnostics can be very verbose on first use; preserve the full log.
    log = OUT / "build.log"
    with log.open("w") as stream:
        result = subprocess.run(["sh", str(SDK / "tools/build-sketch.sh"), str(sketch),
                                 "--board", "pw1_storage", "--out", str(OUT / "dashy.elf")],
                                env=env, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Native build failed; see {log}")
    print(f"Paperwhite 1 program: {OUT / 'dashy.elf'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    prepare() if args.prepare_only else build()
