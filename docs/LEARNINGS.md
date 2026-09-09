# What we learned getting Dashy onto a Paperwhite 1

This is a record of work completed on **9 September 2026**, with one **Paperwhite 1 Wi-Fi (B024)** running **5.6.1.1 (268989035)**. The user confirmed the native dashboard, landscape conversion, reopening fix, and final 180-degree flip. Model prefix and firmware are retained for reproducibility; the full serial, account details, device backups and raw logs are private.

## 1. Choose the runtime before attempting an OS replacement

The initial aim was to repurpose an unsupported Kindle as a dedicated dashboard and remove the normal Kindle experience. We investigated complete OS replacement:

- [Quill's device list](https://github.com/Quill-OS/quill/wiki#currently-supported-devices) included Kindle Touch, but not Paperwhite 1.
- [Ponder](https://forge.solarcene.community/smallsolar/Ponder_installation) describes a replacement OS developed for Paperwhite 3. That does not establish PW1 compatibility.
- [Alpine Kindle](https://github.com/schuhumi/alpine_kindle) runs a chroot on the existing Kindle system, retaining the host OS.

We found no verified, ready-to-install PW1 replacement image in those projects. That is a finding about the checked projects, not proof that replacing the OS is impossible.

The simpler route was [Kinduino 0.5.2](https://github.com/best-effort-labs/kinduino/tree/v0.5.2): run a small native app while its supervisor stops the reader framework. The working kernel, display drivers and hardware services remain. No OS image was flashed or erased. Automatic startup was prepared but left off.

The [original Dashy project](https://github.com/ahdrage/Dashy-Home-Dashboard-for-Waveshare-ESP32-S3-RLCD-4.2) targets an ESP32-S3 and Waveshare display. Its firmware cannot run on a Kindle. We reused its layout and data mapping, then implemented a Python preview and a separate native C++ renderer.

## 2. USB storage access is not root access

A mounted Kindle exposes user storage, not the complete Linux root filesystem. Copying a script there does not execute it or prove the device is unlocked. We first backed up all accessible files, including hidden contents, and verified the copy with SHA-256 hashes. This was a USB backup, not a full flash/recovery image.

After a safe ejection, the cable had to be disconnected and reconnected to expose USB storage again. Drive identifiers can change: identify the mounted Kindle each time rather than reusing an old disk number.

A host backup process once held the mount open. A normal unmount retry resolved it; there was no need to force-kill processes or disable backup software. Complete copies, verify them, then eject normally.

## 3. The old browser needed a local HTTP entry point

The Kindle browser could not reliably open the hosted modern HTTPS setup page. A [PW1 / firmware 5.6.1.1 report](https://github.com/KindleModding/kindlemodding.github.io/issues/196) describes local hosting as a workaround. We ran the [WinterBreak2 server](https://github.com/KindleModding/Winterbreak2) on the Mac and served a fixed device dialog over local HTTP.

The downloaded `.mobi` is intentionally empty. The mechanism is in the download's `Content-Disposition` header: it opens a custom dialog already copied onto USB storage. The dialog requests a fixed local command through the Kindle bridge.

“Web Browser successfully downloaded…” proves that the browser completed the download. It does **not** prove that the command ran or that installation succeeded. A subsequent application error can overlap installer text on the e-ink screen. We used actual logs and installed files to distinguish these cases.

For repeat opening, the dialog uses a timestamped request ID and dismisses itself after two seconds. The native-dialog dismissal API is also described in the [Native Kindle Menu author's post](https://www.mobileread.com/forums/showthread.php?p=2679942).

## 4. The bootstrap's decoder was missing

The first unlock log identified the concrete problem: this firmware had no `base64` executable. The original bootstrap unpack stage depended on:

```text
base64 -d | xz -d | tar ...
```

It reported `base64: not found`, followed by extraction failures. Later stages continued, and a `JAILBROKEN.txt` file appeared even though required components were absent. The marker and screen messages were therefore insufficient evidence of a successful unlock.

The fix in [prepare_pw1_unlock.py](../tools/prepare_pw1_unlock.py) is narrowly scoped:

1. Require the exact SHA-256 of the inspected upstream `jb.sh` 1.3.7.
2. Decode its original base64/XZ payload on the Mac.
3. Validate archive entries and write a plain tar archive for the Kindle.
4. Replace only the six-line unpack stage; preserve the original installer body.
5. Abort before installation if extraction fails or required files are missing.

[The unpack tests](../tests/test_unlock_unpack.py) run just that stage, in isolation, with `base64` absent. A good payload preserves its contents, and a damaged archive cannot reach the installer body. The device wrapper then verifies installed unlock components before invoking Dashy's installer.

The payload's `kindlepw2` directory label is an upstream compatibility path; it does not identify this device as a PW2. We kept explicit PW1 model/firmware checks rather than inferring the model from that name.

## 5. Unlocking and library launchers are separate outcomes

Even after the native app worked, upstream shell-integration registration logged an error. Library scriptlet indexing never became a confirmed launcher on this unit. We therefore did not rely on a library item appearing to establish success.

Typing `install dashy` in the search bar only searched the library. It was not an install command.

The verified route was the local browser dialog calling the installer and later the detached reopen helper directly. The library scriptlets remain in the package for compatible installations. An optional `RUNME.sh` and `;log runme` shortcut were staged, but that offline entry point is still unconfirmed here. See [the upstream scriptlet documentation](https://kindlemodding.org/kindle-dev/scriptlets.html) for the broader mechanism.

## 6. Landscape means rotating pixels and refresh areas

Changing display width and height was insufficient. In the pinned SDK, the 90/270-degree geometry path did not rotate raster data; its fallback remained unrotated. We added [LandscapeBackend.h](../native/dashy/LandscapeBackend.h) around the existing backend.

The first landscape image put the USB/power edge on the right. The user requested a further 180-degree turn. The final image puts that edge on the **left**.

For logical size 1024 × 758 and physical size 758 × 1024:

```text
logical pixel (x, y)       -> physical (y, 1023 - x)
logical area (x, y, w, h)  -> physical (y, 1024 - x - w, h, w)
```

Both conversions matter. Rotating the full image while leaving dirty rectangles unchanged would make clock-only updates corrupt or refresh the wrong area. Host checks compare every rotated pixel and verify that partial clock refreshes leave the date and sensor readings intact.

Input coordinates remain a runtime concern. The original portrait top-left exit corner becomes the **top-right** corner in the final landscape orientation. That mapping follows from the rotation; the gesture still needs systematic physical verification.

## 7. A working browser did not mean the previous runtime was gone

After the landscape update, tapping the browser button could appear to do nothing. The relevant log said:

```text
exit the running Kinduino app before installing
```

The reader/browser had returned, but `/var/local/kinduino/run/current` still named Dashy. The installer correctly refused to overwrite a possibly active app. A leftover session marker was the cause in the observed stalled-launch case.

The fix in [stop.sh](../native/device/stop.sh) checks both the marker and runtime processes. For a live Dashy session, it finds the supervisor by its full argument list, avoids confusing its same-command subshell with the parent, requests TERM, and waits up to 45 seconds for cleanup. Only after all related runtime processes are absent can it remove a stale marker.

Another running app, orphaned runtime processes, or a supervisor that does not stop prevents the update. Blindly deleting the marker or force-killing everything would hide the real state. Tests exercise all of these branches, including graceful termination with a disposable host worker.

The user confirmed reopening worked after the fix.

## 8. Detach the entire operation before the reader stops

The reader interface may restart or stop while handling an unlock, install or launch. A child operation still attached to that interface can be terminated midway.

[native/browser-launch.sh](../native/browser-launch.sh) detaches the **whole** update operation using Kinduino's detacher. The application launcher separately detaches the supervisor. The resulting sequence is stop, verify inactivity, install a pending upload, then launch.

The runtime's framework shim owns the `com.lab126.kaf` service name. Aggressively killing it and immediately restarting the reader can break that service handoff. The shipped Dashy controls prefer orderly supervisor cleanup. A stuck or orphaned runtime stops the update and calls for recovery; it is not silently ignored.

## 9. Small rendering and packaging choices matter on e-ink

The original design includes time and sensor columns, but a seconds display would cause unnecessary refreshes. The native app redraws the clock once per minute and requests a full refresh every 15 minutes, when the date changes, or when the clock jumps. It uses the Kindle's clock, Oslo DST rules, and a prompt for an obviously unset date. It does not yet synchronize time or sleep between updates; battery life has not been measured.

The native font renderer needs static fonts. The build creates Montserrat weights 400 and 500, renames them Dashy Sans, and carries the OFL notice.

The runtime needs manifest version 2 and API 2.0.0, with executable/asset hashes, lengths and a `READY` marker written last. The complete upload is validated by the actual runtime installer. A missing font or corrupted executable must fail without losing the recoverable upload.

Host tests exposed a macOS/Linux difference when renaming read-only directories. The installer test harness adapts that host behavior; the shipped Kindle runtime is unchanged.

## 10. Keep setup paths explicit

Express's default dotfile policy blocked the fixed placeholder when it lived under the hidden `.deps` directory. We permitted that one explicitly selected file, not arbitrary file access, and removed external web-font requests so the page remained local.

The public server resolves its asset relative to its source file, supports explicit `launch` and `unlock` modes, and defaults to loopback. Both modes have HTTP tests. These public packaging refinements have not had a separate physical-device test; the underlying dialog commands and launch flow are the ones used during setup.

There are two different servers: the optional Python **display preview** on port 8787 and the Node **device launcher** on port 3000. The native app needs neither to keep drawing after launch. The currently confirmed reopening route needs the launcher computer on the same Wi-Fi.

## Confirmed build

| Item | Value |
| --- | --- |
| Hardware / firmware | PW1 Wi-Fi, B024, 5.6.1.1 (268989035) |
| Final orientation | Landscape, 1024 × 758, USB/power edge left |
| Native build ID | `dashy-demo-9bcf8a148a81e265` |
| Executable SHA-256 | `d095343da5a51a8264b73ed010e4064e3f51d8b066625f9874b38af18f29c374` |
| SDK revision | `f77ef002faf1cae520db4038d9c4ee4a3c01fef4` (Kinduino 0.5.2) |
| Compiler | Zig 0.16.0, static ARM build |
| Confirmation | User confirmed final orientation and operation on 9 September 2026 |

Build dependency URLs and checksums are in [native/README.md](../native/README.md). The original bootstrap hash and preparation steps are in [setup/README.md](../setup/README.md). The original Dashy reference was inspected at `8252b6c881a326facea394f824c0a6aa1007d7e5`; see [NOTICE.md](../NOTICE.md).

## Remaining work

- Physically verify exit/recovery, repeated sleep/wake, minute/full refresh timing, and long unattended operation.
- Measure battery use and consider an RTC-based sleep strategy.
- Confirm a reliable offline launcher.
- Inspect boot behavior and validate the supplied opt-in startup job through a complete reboot.
- Add calendar data and clock synchronization. Live Netatmo was implemented in the follow-up below.
- Extend model/firmware support only after checking the corresponding display, runtime and recovery behavior.

The current result is a working native dummy dashboard with a confirmed landscape orientation and reopening path. It is not yet a fully validated unattended appliance.

## Netatmo follow-up

The native app now follows the original Dashy refresh-token flow and three-minute station cadence. The same C++ OAuth, BearSSL TLS and private token store successfully fetched real account readings on the Mac. The user subsequently confirmed live readings on the physical Kindle after installing the update.

Credentials are entered in an ignored private file, checked without printing them, and staged separately from the application archive. Replacement refresh tokens are saved atomically and survive reinstallation. HTTPS verifies Netatmo’s hostname, certificate chain and expiry using a fingerprint-checked DigiCert root, instead of the original firmware’s insecure TLS mode.

A worker keeps network delays away from the clock and exit handling. Host tests deliberately block a request while checking that display reads remain responsive. Updated values and connection messages redraw just the data region, with rotation checks comparing it against a full redraw. [Setup and operating details](NETATMO.md).
