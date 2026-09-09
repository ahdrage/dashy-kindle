# Local setup and launcher

These files preserve the route that worked on **Paperwhite 1 Wi-Fi (B024), firmware 5.6.1.1**. Use them only with that checked device/firmware combination. The wrapper intentionally rejects other models and firmware. Read the [current upstream WinterBreak2 guide](https://kindlemodding.org/jailbreaking/WinterBreak2/) before a new installation; this repository records a pinned, tested setup rather than tracking all upstream changes.

The server is adapted from [WinterBreak2](https://github.com/KindleModding/Winterbreak2), commit `82167878e27229789a0cef50c85d9e8d897cc7c6`. Its local HTTP page works around this Kindle browser's HTTPS trouble. It sends an intentionally empty `.mobi` with the upstream dialog mechanism in its download header. The actual script and HTML dialog are already on the Kindle.

## Reopen an installed dashboard

Prerequisites: the initial unlock succeeded, Kinduino 0.5.2 and Dashy are installed, and the [current bundle files](../native/INSTALL.md#copy-the-bundle) were copied, including `winterbreak2/dialoger-launch.html` and `dashy/apply-landscape.sh`.

Use Node.js 22+ and npm. From this repository:

```sh
cd setup/server
npm ci --ignore-scripts
HOST=0.0.0.0 npm start
```

In the Kindle Experimental Browser, open `http://HOST_IP:3000/`, using the computer's local Wi-Fi address, and tap **Open Dashy**. Keep the computer awake while opening. Allow up to one minute; the app runs locally after launch. Stop the server with Ctrl-C when finished. Without `HOST=0.0.0.0`, the server listens only on the computer's loopback address.

The default mode launches an already installed app. It does not rerun the jailbreak. A timestamp gives each launch request a new ID, and the custom dialog dismisses itself after two seconds. The native updater handles stopping a previous session and installing a pending update.

Use this server only on a trusted local network. It is a setup tool with a fixed privileged device entry point, not an Internet service. It does not accept arbitrary filenames, dialog names, or shell commands from the browser.

## First-time unlock

This step modifies the Kindle system. It is distinct from copying a PDF, building the app, or reopening Dashy. Back up accessible USB storage first; that copy is not a complete firmware recovery image. The model and firmware guards must remain in place.

The official `jb.sh` **1.3.7** used here expected a `base64` command that this firmware lacked. `tools/prepare_pw1_unlock.py` checks the exact original hash, decodes its archive on the Mac, and changes only the six-line unpacking stage. It keeps the installer body unchanged and aborts before installing if archive extraction or required assets fail.

1. [Build and package Dashy](../native/README.md).
2. Download and prepare the pinned bootstrap from the repository root:

```sh
set -eu
mkdir -p .deps
curl -fL https://kindlemodding.org/jb.sh -o .deps/winterbreak2-jb.sh
.venv/bin/python tools/prepare_pw1_unlock.py
cp setup/device/run.sh setup/device/dialoger-pw1-retry.html setup/device/dialoger-launch.html output/unlock/winterbreak2/
.venv/bin/python -m unittest discover -s tests -p 'test_unlock_unpack.py' -v
```

Expected upstream SHA-256: `65a63528fbe9515950cc3aa0d931749548680f37898a3819a4ebc0a740588942`. The download URL can change over time. **If the hash no longer matches, stop and review the new upstream version.** Do not merely change the expected hash. The payload is downloaded separately and is not included in Git.

The two unpack tests execute only the adapted unpack stage in a temporary directory with `base64` absent. They verify a valid archive is preserved and a damaged one cannot reach the system-changing installer. They do not run the jailbreak on the Mac.

3. Copy the Dashy bundle as described in [installation](../native/INSTALL.md#copy-the-bundle). Merge the contents of `output/unlock/winterbreak2/` into the Kindle USB-root `winterbreak2/` directory. It must include `kmc.tar`, `jb.sh`, `run.sh`, `dialoger-pw1-retry.html`, and `dialoger-launch.html`. `output/unlock/pw1-retry.json` records hashes of the adapted payload; verify the copied files before ejecting.
4. Safely eject the Kindle and return to its Experimental Browser. Start the server in unlock mode:

```sh
cd setup/server
npm ci --ignore-scripts
DASHY_SETUP_MODE=unlock HOST=0.0.0.0 npm start
```

5. Open `http://HOST_IP:3000/` on the Kindle and tap **Unlock and install Dashy** once. The wrapper checks root access, model and firmware, runs the adapted bootstrap, verifies actual installed files, installs Kinduino/Dashy, then requests the native launch. The interface can restart during this process.
6. Confirm the dashboard appears. If it does not, reconnect USB and inspect `winterbreak2/jailbreak-retry.log` and `dashy/install.log`. A download-success message or `JAILBROKEN.txt` alone is insufficient. Do not repeatedly run the unlock to fix a launch problem.
7. Stop the server and restart in its default launch mode for future use.

On an already unlocked device with a working root entry point, install the runtime and app through the [normal installation steps](../native/INSTALL.md#first-installation); the first-time bootstrap is unnecessary.

## Files and verification

| File | Role |
| --- | --- |
| `server/api/index.js` | Fixed launch/unlock HTTP responses |
| `server/assets/placeholder.mobi` | Intentionally empty download; not a real ebook |
| `device/dialoger-pw1-retry.html` | Invokes the model-checked unlock wrapper |
| `device/run.sh` | Checks device, runs adapted bootstrap, verifies files, installs and opens Dashy |
| `device/dialoger-launch.html` | Invokes the detached Dashy update/reopen helper |
| `../tools/prepare_pw1_unlock.py` | Verifies and predecodes the original bootstrap |

```sh
cd setup/server
npm test
```

Server checks verify both modes, the fixed header, the empty asset from any working directory, non-public project files and rejection of an unknown mode. The public server adds an explicit mode selector and loopback default to the proven local mechanism; these packaging changes have host coverage, not an additional physical Kindle test.

See [the learning log](../docs/LEARNINGS.md) for the investigation and [attribution](server/NOTICE.md) for upstream credits.
