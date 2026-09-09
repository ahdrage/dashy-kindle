# Dashy for Kindle

A native e-ink dashboard for the **Kindle Paperwhite 1**, inspired by [Dashy for the Waveshare ESP32-S3 RLCD](https://github.com/ahdrage/Dashy-Home-Dashboard-for-Waveshare-ESP32-S3-RLCD-4.2).

![Native dashboard, rendered from the same C++ used on the Kindle](docs/assets/dashboard.png)

An Oslo clock, Norwegian date, and three sensor columns: **INNE | CO2 | UTE**. The native app reads **live Netatmo data every three minutes**, using privately configured OAuth credentials. The screenshot and Python preview use sample values. See [Netatmo setup](docs/NETATMO.md).

**Confirmed working on 9 September 2026:** Paperwhite 1 Wi-Fi (model prefix B024), firmware **5.6.1.1 (268989035)**. The final layout is **1024 × 758 landscape, with the USB/power edge on the left**. Portrait, landscape, the final 180-degree flip, and reopening through the local browser launcher were confirmed on the physical device. The image above is a host render of the native C++ framebuffer.

## How it runs

Dashy runs on the Kindle through **Kinduino 0.5.2**. Kinduino stops Amazon's reader interface while the app runs and uses the existing Linux system and display drivers. **This is an application and runtime, not a replacement OS image.** The selected approach does not erase the Kindle OS.

Once open, the clock and Netatmo dashboard run on the Kindle without a Mac serving images. Live readings require the Kindle’s Wi-Fi connection. The currently verified way to reopen it uses a small HTTP launcher on a Mac on the same Wi-Fi. An offline shortcut and automatic startup are included but still need physical validation; startup is disabled by default.

## Start here

1. [Build the native app](native/README.md), including exact dependency versions and checksums.
2. [Prepare the first-time unlock](setup/README.md#first-time-unlock), only if the supported device is not already unlocked.
3. [Install and reopen Dashy](native/INSTALL.md), including updates, exit, and recovery.

To try the design without changing a Kindle, use the [Python browser preview and PDF export](docs/PREVIEW.md).

Read [what we learned](docs/LEARNINGS.md) for the missing decoder, misleading download messages, stale session marker, launch fix, and display rotation. [Architecture](docs/ARCHITECTURE.md) explains the renderer and runtime boundary.

## What is verified

| Area | Status |
| --- | --- |
| PW1 native display, landscape, final flip, browser reopening | Confirmed on the device |
| Time formatting, daylight saving, refresh decisions, rotated pixels and dirty rectangles | Automated host checks pass |
| Runtime installation, damaged payload rejection, graceful stop and stale-session handling | Automated host checks pass; reopening also confirmed on the device |
| Exit gesture, sleep/wake, battery life, long unattended operation | Not systematically verified on the device |
| Automatic boot startup | Implemented, disabled, not physically validated |
| Library scriptlets and `;log runme` offline shortcut | Included; not confirmed as working entry points on this device |
| Netatmo OAuth, HTTPS, refresh-token persistence and station readings | Confirmed with live account data on the host and physical Kindle |
| Calendar events and automatic clock synchronization | Not implemented |

This is a record of one tested hardware/firmware combination, not a general compatibility claim for other Kindles. Future builds still need their own physical checks.

## Source map

| Path | Purpose |
| --- | --- |
| `native/dashy/` | C++ dashboard, clock and landscape rotation |
| `native/device/` | Installation, graceful stop, update and optional boot controls |
| `native/scriptlets/`, `native/RUNME.sh` | Device entry points |
| `setup/` | Local HTTP launcher and device dialog files |
| `tools/` | Pinned native build, packaging and PW1 bootstrap adaptation |
| `dashy.py`, `web/` | Optional browser preview and static PDF export |
| `sample-data.json` | Netatmo-shaped dummy readings |
| `tests/`, `native/tests/`, `setup/server/test/` | Host verification |
| `docs/` | Architecture, setup lessons and preview guide |

The Python preview rereads `sample-data.json` on each request. The native app reads Netatmo and shows placeholders until connected. Credentials and replacement refresh tokens stay outside source control and installation archives.

## License and attribution

The Dashy application is MIT licensed. The setup server is adapted from WinterBreak2, whose package declares ISC. Fonts use the SIL Open Font License. The Kinduino SDK is MIT licensed; the device bundle also preserves its separately licensed runtime components, including FBInk's GPL license and source offer. See [NOTICE.md](NOTICE.md), [LICENSE](LICENSE), and [setup/server/NOTICE.md](setup/server/NOTICE.md).

Device backups, account information, books, raw logs, downloaded dependencies, and generated installation archives are excluded from source control.
