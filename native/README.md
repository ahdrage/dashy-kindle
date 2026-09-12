# Build native Dashy

The C++ program runs on **Paperwhite 1 Wi-Fi (B024), firmware 5.6.1.1** through **Kinduino 0.5.2**. Default orientation is landscape with the USB/power edge on the left. See [architecture](../docs/ARCHITECTURE.md) for rendering and refresh details.

## Fresh checkout

The build script currently targets an **Apple Silicon Mac**, with Python 3.9+, Git, curl, and the Xcode command-line C/C++ tools. Other build hosts have not been configured or tested. Dependencies live in ignored `.deps/`; they are not vendored.

```sh
git clone https://github.com/ahdrage/dashy-kindle.git
cd dashy-kindle
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
```

Download the pinned SDK, compiler and device runtime. This block stops on a checksum or revision mismatch:

```sh
set -eu
mkdir -p .deps
git clone --depth 1 --branch v0.5.2 https://github.com/best-effort-labs/kinduino.git .deps/kinduino
test "$(git -C .deps/kinduino rev-parse HEAD)" = f77ef002faf1cae520db4038d9c4ee4a3c01fef4
curl -fL https://ziglang.org/download/0.16.0/zig-aarch64-macos-0.16.0.tar.xz -o .deps/zig-aarch64-macos-0.16.0.tar.xz
printf '%s  %s\n' b23d70deaa879b5c2d486ed3316f7eaa53e84acf6fc9cc747de152450d401489 .deps/zig-aarch64-macos-0.16.0.tar.xz | shasum -a 256 -c -
tar -xf .deps/zig-aarch64-macos-0.16.0.tar.xz -C .deps
curl -fL https://github.com/best-effort-labs/kinduino/releases/download/v0.5.2/kinduino-device-0.5.2.zip -o .deps/kinduino-device-0.5.2.zip
printf '%s  %s\n' 3cd7e4ef1665e38b626616e4764eb8e6f6a4486a351a629cdae1c401df2c008d .deps/kinduino-device-0.5.2.zip | shasum -a 256 -c -
```

For an existing checkout, reuse these dependencies only after verifying the same revisions and checksums. Do not replace a pin just to bypass a failed verification.

## Build and verify

Run from the repository root:

```sh
.venv/bin/python tools/build_native.py
sh tools/test_night.sh
sh tools/test_native.sh
sh tools/test_netatmo.sh
.venv/bin/python tools/package_native.py
.venv/bin/python -m unittest discover -s native/tests -p 'test_bundle.py' -v
.venv/bin/python -m unittest discover -s tests -v
```

`build_native.py` prepares static font instances and the verified public TLS root certificate, then cross-compiles for the SDK's `pw1_storage` board. It verifies the SDK commit. `package_native.py` checks the runtime archive checksum, ARM executable format, manifest and asset hashes. Automatic startup is disabled in every generated package.

The first compiler build can take several minutes and emit upstream nullability warnings. Detailed diagnostics are saved in `output/native/build.log`. Use `--prepare-only` to generate assets without cross-compiling.

The Montserrat variable font is instantiated at weights 400 and 500 for the native font renderer. These derivatives are named **Dashy Sans** and retain their OFL license.

## Outputs

| File | Purpose |
| --- | --- |
| `output/native/dashy.elf` | Static, stripped 32-bit ARM EABI5 executable |
| `output/native/dashy-native-landscape.pgm` | Exact 1024 × 758 logical framebuffer from host rendering tests |
| `output/native/dashy-native.pgm` | Portrait rendering regression reference |
| `output/native/Dashy-PW1-demo.zip` | Runtime, app, fonts, controls, launch dialog, source and licenses |

The archive includes `build-info.json` and `SHA256SUMS`. New packages use `physical_device_tested: false`; building successfully is not physical verification. The program confirmed in this project is recorded separately in [the learning log](../docs/LEARNINGS.md#confirmed-build).

## What the checks cover

The native renderer tests cover Norwegian formatting, Oslo winter/summer time and both 2026 DST transitions, midnight, unset clocks, minute updates, time corrections, pixel bounds, and full refresh decisions. Every rotated landscape pixel is compared with the physical framebuffer, including partial updates that must leave the sensor readings untouched.

The device-control tests use isolated temporary filesystems and substitute device tools. They cover startup gates, once-per-boot behavior, model guards, stale session markers, graceful supervisor termination, other running apps, orphaned processes and stop timeouts.

The night-mode tests cover 23:00 and 07:00 boundaries, midnight/year rollover, seven- and nine-hour DST nights, durable test results, interrupted or inhibited sleep, and cancellation of a pending network pause at morning. The Netatmo worker test checks that an in-flight request prevents sleep and that an eight-hour suspend advances OAuth expiry. Renderer checks verify that clearing the night screen is followed by a complete redraw even within the same minute.

The bundle tests compile the **actual Kinduino installer C code** for the host. They check acceptance of the complete ARM payload and rejection of a damaged executable or missing font. A host-only adapter handles macOS/Linux differences when renaming read-only directories. The shipped runtime is unchanged, and the ARM program is never executed on the Mac.

Unlock unpack tests are optional: they skip until the separately downloaded upstream payload is prepared. [Setup instructions](../setup/README.md) explain how to run them without executing any system-changing installer code on the host.

See [INSTALL.md](INSTALL.md) for installation and recovery. Host checks do not establish battery life, physical exit behavior, sleep/wake or boot reliability.

See [night-mode setup](../docs/NIGHT-MODE.md) for the one-minute on-device test that gates the 23:00–07:00 schedule. This schedule is independent of automatic boot startup, which remains disabled.

## Live Netatmo

See [Netatmo setup](../docs/NETATMO.md). The same OAuth client and BearSSL HTTPS transport are used by the Kindle and `tools/check_netatmo.py`. Tests cover token rotation/restart, expiry, one authorization retry, rate limits, missing/invalid readings, stale data, private atomic storage and a blocked network request that must not block the display thread. No private settings are compiled into the executable or packaged as assets.
