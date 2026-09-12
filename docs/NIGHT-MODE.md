# Automatic night mode

Dashy sleeps every night from **23:00 to 07:00, Oslo time**. At bedtime it clears the e-ink screen, saves the current front-light level, switches the light off, and asks Kinduino for timed suspend-to-RAM. At 07:00 it restores the light, redraws the complete dashboard and requests fresh Netatmo readings. The Mac is not needed for this schedule once Dashy is running.

This is timed sleep within the running app. Automatic launch after a full shutdown or reboot is still a separate, disabled feature. The Kindle system clock must be correct; Dashy applies Oslo's summer/winter-time rules itself. Spring's clock-change night sleeps seven hours and autumn's sleeps nine.

## First launch and hardware check

Install the updated bundle and open Dashy as usual, with USB unplugged. On the first launch:

1. Dashy displays its usual readings and a notice about the test.
2. After at least ten seconds, it waits for any Netatmo request and refresh-token save to finish.
3. The screen clears and the light goes off for a **60-second sleep test**. Leave the power button alone during the test.
4. After an automatic wake, the screen and light return. **NATT 23–07** in the footer means the test passed and the nightly schedule is enabled.

The test compares elapsed wall time against the monotonic clock, which stops during suspend on this Kindle. A simulated delay, USB-inhibited suspend, failed alarm, or early manual wake does not count as a pass. Failure leaves the regular dashboard running with **NATTMODUS IKKE AKTIV**; it does not keep retrying sleep. If the screen remains blank for two minutes, press the power button once and reconnect USB to investigate.

Test status is stored in `/var/local/kinduino/sketches/dashy/files/night-mode.state`. `enabled` means the test passed. `testing` records an interrupted attempt, and `failed` disables further sleep attempts. The file survives app updates and relaunches. To repeat a failed test, close Dashy and remove only this state file through the existing device shell; never remove `files/netatmo.json`. Unknown or symlinked state files also disable night mode.

The updater copies runtime diagnostics to USB `dashy/runtime-launch.log` after its launch wait. Lines beginning `Dashy night:` record the test and sleep requests. If reconnecting early, that snapshot may still be from the preceding launch.

## Network and early wakes

The network worker acknowledges a pause only after the current request finishes and any rotated refresh token has been saved. A failed token save keeps the device awake so the only current token is not stranded by an unattended sleep. Suspend stops execution of the network worker; the OS manages Wi-Fi suspension and reassociation.

On wake, suspended time is added to the worker's monotonic elapsed time so an OAuth token that expired overnight is renewed. The worker allows up to 30 seconds for Wi-Fi to reconnect before its usual three-minute retry interval. An existing Netatmo rate-limit delay is still respected.

An early power-button wake during scheduled night mode keeps the screen blank and gives the existing corner-hold exit gesture a brief window before sleeping again until 07:00. At morning, a pending network pause is cancelled even if a slow request prevented the device from entering sleep at all.

## Verification

Host checks cover bedtime and morning boundaries, midnight/year rollover, both DST changes, one-time test persistence, failed/inhibited sleep, safe request pausing, token expiry across suspend and complete screen redraw. Run `sh tools/test_night.sh`, `sh tools/test_netatmo.sh` and `sh tools/test_native.sh`.

The ARM build has compiled successfully. Physical timed wake and overnight endurance still need device confirmation for this build. Battery savings have not been measured.

Prepared on 12 September 2026: build `dashy-night-a743af3376df3cd1`, program SHA-256 `7fa6f2072085d72c922440b5f06efc69af65713e2dc1c4992e0303392dcb8fb4`. This is the build staged for the one-minute physical test.
