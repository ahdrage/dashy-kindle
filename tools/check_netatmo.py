"""Check private credentials using the Kindle's actual TLS/OAuth code; persist token rotation."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / ".deps/kinduino"
PIN = "f77ef002faf1cae520db4038d9c4ee4a3c01fef4"
OUT = ROOT / "output/native/netatmo-host"


def build():
    if subprocess.check_output(["git", "-C", str(SDK), "rev-parse", "HEAD"], text=True).strip() != PIN:
        raise RuntimeError("Use the pinned Kinduino SDK; see native/README.md")
    OUT.mkdir(parents=True, exist_ok=True)
    library = SDK / "arduino/libraries"
    bear = library / "BearSSL"
    archive = OUT / "libbearssl.a"
    if not archive.exists():
        sources = sorted((bear / "src").rglob("*.c"))
        def compile_one(source):
            obj = OUT / (str(source.relative_to(bear)).replace("/", "_") + ".o")
            subprocess.run(["cc", "-O2", "-I"+str(bear / "inc"), "-I"+str(bear / "src"),
                            "-c", str(source), "-o", str(obj)], check=True, capture_output=True)
            return obj
        with ThreadPoolExecutor(max_workers=8) as pool:
            objects = list(pool.map(compile_one, sources))
        subprocess.run(["ar", "rcs", str(archive), *map(str, objects)], check=True, capture_output=True)
    core = SDK / "arduino/cores/kindle"
    sources = [ROOT / "native/tests/netatmo_check.cpp"]
    sources += [ROOT / "native/dashy" / name for name in ("Netatmo.cpp", "NetatmoStore.cpp", "NetatmoHttp.cpp")]
    sources += [core / name for name in ("Print.cpp", "Stream.cpp", "String.cpp", "IPAddress.cpp", "time.cpp")]
    sources += [library / "WiFi" / name for name in ("WiFiClient.cpp", "WiFiClientSecure.cpp")]
    sources += [library / "HTTPClient/HTTPClient.cpp"]
    includes = [ROOT / "native/dashy", core, library / "ArduinoJson", library / "WiFi", library / "HTTPClient", bear / "inc"]
    binary = OUT / "netatmo-check"
    subprocess.run(["c++", "-O2", "-std=c++17", "-pthread", *["-I"+str(p) for p in includes],
                    *map(str, sources), str(archive), "-o", str(binary)], check=True)
    return binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--credentials", type=Path, default=ROOT / "secrets/netatmo.json")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    binary = build()
    if args.build_only:
        print("Host Netatmo checker built; no API requests made.")
        return 0
    return subprocess.run([str(binary), str(args.credentials.resolve()),
                           str(ROOT / "assets/certs/DigiCertGlobalRootG2.crt")]).returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        # Build commands contain source paths only, never tokens or request bodies.
        print("Could not build the Netatmo checker:", error, file=sys.stderr)
        sys.exit(2)
