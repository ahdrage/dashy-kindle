# Install and reopen Dashy

Tested on **Kindle Paperwhite 1 Wi-Fi (B024), firmware 5.6.1.1 (268989035)**. The final 1024 × 758 landscape dashboard has the **USB/power edge on the left**. Its display and browser reopening were confirmed working on 9 September 2026.

The app shows an Oslo clock, Norwegian date and live Netatmo readings. Once open, it runs on the Kindle without a Mac serving images, using the Kindle’s Wi-Fi. It uses the Kindle’s system clock; a date before 2020 displays “Still klokken på Kindle”. Automatic clock synchronization is not implemented. The original demo and subsequent live Netatmo update were both confirmed on the physical device.

Amazon's reader interface stops while Dashy runs, but the existing OS and hardware drivers remain. The archive is an application/runtime bundle and does not itself unlock the Kindle.

## First installation

1. Confirm the exact hardware and firmware. Back up all accessible USB contents, including hidden files. A USB backup is not a full firmware recovery image.
2. [Build the bundle](README.md), then extract `output/native/Dashy-PW1-demo.zip` on the computer.
3. If the device is not unlocked, follow [first-time unlock](../setup/README.md#first-time-unlock). That flow stages this bundle and installs and launches it after verifying the unlock files.
4. If the device is already unlocked, copy the bundle as described below, safely eject, then run `sh '/mnt/us/documents/Install Dashy.sh'` through an existing trusted root entry point. This installs Kinduino and the app. The browser reopening button requires that runtime to exist; it is not the initial runtime installer.
5. Use the [local browser launcher](../setup/README.md#reopen-an-installed-dashboard). The library's **Open Dashy** scriptlet is also provided, but library indexing did not become a confirmed entry point on this unit.

Do not repeat the jailbreak just to reopen or update an installed dashboard.

## Copy the bundle

Copy the **contents** of the archive's `USB-ROOT` directory to the Kindle USB drive's root. Include the hidden `.kinduino` directory. Merge `extensions` with existing extensions. Preserve unrelated files, any existing `RUNME.sh`, and another pending `.kinduino` upload before proceeding.

Copy all application files and font assets first, then `.kinduino/manifest`, and **`.kinduino/READY` last**. Verify the copied files against the archive's `SHA256SUMS` before safely ejecting. The upload is consumed when the runtime installs it. The controls live in `extensions/dashy`, and the runtime installer lives in `extensions/kinduino`.

The bundle includes `winterbreak2/dialoger-launch.html` and `dashy/apply-landscape.sh`. The latter is the historical device name of the general reopen/update entry point; it now supports the final orientation. It is packaged from `native/browser-launch.sh`.

## Netatmo credentials

Follow [Netatmo setup](../docs/NETATMO.md) to check the credentials privately and stage them separately from the app bundle. At the next launch, the updater moves them into app storage and removes the USB staging copy. Later app updates preserve the current refresh token.

## Reopening and updating

Start the [local launch server](../setup/README.md#reopen-an-installed-dashboard) on the computer. In the Kindle Experimental Browser, open `http://HOST_IP:3000/` and tap **Open Dashy**, replacing `HOST_IP` with the computer's address on the same Wi-Fi. Allow up to one minute. A “successfully downloaded” dialog alone does not confirm that the app launched.

The launcher closes a previous Dashy session gracefully, clears a leftover marker only when the runtime is inactive, applies a pending upload, and opens Dashy. For an update, first copy a newly built bundle while USB storage is available, safely eject, then use the same button.

The night-mode update adds a one-time, 60-second sleep test after the dashboard first appears. Allow extra time for that test, with USB unplugged, as described below. Application updates preserve both the current Netatmo credentials and any completed night-mode test result.

For an optional offline shortcut, enter **`;log runme`** in the Kindle home search bar. The bundled `RUNME.sh` uses the installed upstream dispatcher, but this path is not yet confirmed on the physical device. Entering ordinary text such as `install dashy` only performs a library search; it is not an install command.

## Automatic night mode: 23:00–07:00

After installing a build with night mode, open Dashy during the day and leave it untouched. It first shows the dashboard, then clears the screen and switches off the light for a one-minute hardware sleep test. Do not press the power button or reconnect USB during that minute.

When Dashy returns, **NATT 23–07** in the footer means it detected a successful timed wake and enabled the schedule. It will then sleep from 23:00 until 07:00 every day in Oslo time, including daylight-saving changes. At morning it restores the previous light level, redraws the whole dashboard and refreshes Netatmo. The Mac is not needed while this runs.

**NATTMODUS IKKE AKTIV** means the test or a later sleep attempt failed, or the test was interrupted. The normal dashboard remains available. If the screen stays blank for more than two minutes during the test, press the power button once and reconnect USB to inspect the logs. See [night-mode operation, troubleshooting and test reset](../docs/NIGHT-MODE.md).

Night mode operates while Dashy is running. It does not enable the separate automatic startup option below, and a full reboot still requires reopening Dashy. The one-minute hardware test is not a substitute for checking a complete overnight cycle.

## Check the device

Confirm the complete screen, correct local date/time, a minute update, and a full refresh after 15 minutes. Check unplugged operation, sleep/wake, repeated exit/relaunch, and battery use before relying on unattended operation.

The runtime's exit gesture is a roughly two-second hold of the original portrait top-left corner: **top-right in this landscape orientation**. It should restore the reader interface. This physical gesture and recovery path still need systematic validation on this unit. No in-app rotation control is implemented.

## Troubleshooting and logs

Reconnect USB to read these paths on the mounted drive:

| Path | Purpose |
| --- | --- |
| `winterbreak2/jailbreak-retry.log` | First-time unlock, file verification and install |
| `dashy/install.log` | Runtime and app installation |
| `dashy/landscape-update.log` | Browser entry point and detached update request |
| `dashy/launch.log` | Stop, update and relaunch sequence |
| `dashy/runtime-launch.log` | Saved runtime diagnostics |

These logs can contain device details; redact them before sharing. If the launcher says to exit a running app, use the current update helper rather than deleting `run/current` by hand. It distinguishes a stale marker from a live runtime.

For night mode, look for `Dashy night:` lines in `dashy/runtime-launch.log`. The test records elapsed and suspended seconds and whether it enabled the 23:00–07:00 schedule. The file is a snapshot taken after the launch wait, so reconnecting too early can show the preceding launch instead.

A blank browser after the download can mean the dialog did not run, an install failed, or the launch is still in progress. Read the logs before retrying the unlock. See [the learning log](../docs/LEARNINGS.md) for the specific failures encountered here.

## Automatic startup: implemented, off by default

This is optional and has **not** been validated through a physical reboot. First verify the display, corner exit, sleep/wake and recovery, and inspect the actual `/etc/upstart/framework.conf` on the device. The supplied job targets `started framework` and attempts Dashy once per boot.

Then, through an established root connection:

```sh
sh /mnt/us/extensions/dashy/control.sh enable --tested
```

This briefly makes the system partition writable to install `/etc/upstart/dashy.conf`, then restores read-only mode. It preserves the reader's job and refuses to overwrite an unrelated existing `dashy.conf`. Reboot and verify both startup and exit. The reader may appear briefly before Dashy opens.

To prevent startup, create an empty **`dashy/DISABLE`** file on the USB drive, safely eject and reboot. Removing `dashy/autostart-enabled` also prevents startup. From root, use:

```sh
sh /mnt/us/extensions/dashy/control.sh disable
```

If the display or reader interface is stuck, leave startup disabled and reboot. Avoid forcibly killing the framework shim and immediately restarting the reader; orderly supervisor cleanup is needed to release its framework service.
