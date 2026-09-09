# Architecture

The native app and optional Python preview share the display design and Netatmo field mapping. They are independent implementations; the native app does not request images from Python.

```mermaid
flowchart TD
    A[sample-data.json] --> B[Build demo labels and static fonts]
    B --> C[ARM executable and validated upload manifest]
    C --> D[Kinduino installer and supervisor]
    D --> E[Native C++ dashboard]
    E --> F[Landscape pixel and refresh-area rotation]
    F --> G[Kinduino display protocol]
    G --> H[Separate FBInk service]
    H --> I[Kindle e-ink panel]
```

## Rendering and time

`Dashboard.cpp` draws the same scene in device and host tests. The panel is physically 758 × 1024; the application renders a logical 1024 × 758 scene. `LandscapeBackend.h` converts both pixels and dirty rectangles to panel coordinates, so a partial clock update refreshes the correct physical region.

For the final USB-edge-left orientation, a logical pixel `(x, y)` maps to physical `(y, 1023 - x)`. A rectangle `(x, y, w, h)` maps to `(y, 1024 - x - w, h, w)`. Within its rotated buffer, source `(column, row)` maps to index `(w - 1 - column) * h + row`, with row stride `h`. The app leaves the system framebuffer geometry and reader orientation alone.

The clock uses the Kindle system time and POSIX Oslo daylight-saving rules. Dates before 2020 produce a clock-setting prompt. It polls every 200 ms, redraws the clock at minute boundaries, and requests a full refresh every 15 minutes, on a date change, or after a time jump. There are no seconds or live sensor requests. This version uses the runtime's awake-display policy, not RTC sleep between updates.

## Launching and updating

The local browser button opens a fixed custom dialog already copied to the Kindle. That dialog runs `dashy/apply-landscape.sh`, the historical device filename for `native/browser-launch.sh`.

The launcher detaches the **whole update operation** before the reader interface can restart. The update then:

1. Checks the exact model, firmware, and runtime.
2. Requests a graceful stop of an existing Dashy supervisor and waits for its runtime processes to exit.
3. Removes a leftover `run/current` marker only when no runtime process remains.
4. Installs a pending upload, if present, using the runtime's own validator.
5. Starts Dashy through the detached application launcher and saves diagnostic logs on USB storage.

Another app, an orphaned runtime process, or a stop timeout blocks the update. The code does not clear the session marker simply because reopening failed.

## Upload contract

The `.kinduino` upload contains a static ARM EABI executable, font assets, a version-2 manifest using API 2.0.0, SHA-256 hashes and sizes, and a `READY` marker. Copy `READY` last. The installer verifies the archive contents and consumes the upload when installed. Reopening with no pending upload starts the already installed app.

The packager checks ARM architecture and absence of a dynamic loader, pins the runtime checksum, and includes source and third-party notices. Host tests compile the actual runtime installation code; they do not execute the ARM program.

## Runtime and recovery boundary

Kinduino owns stopping and restoring the reader framework, input handling, the display service, and process supervision. Its framework shim holds the `com.lab126.kaf` service name. Abruptly killing pieces and immediately restarting the reader can leave the service handoff broken; Dashy requests orderly supervisor cleanup instead.

Automatic startup is opt-in. It adds its own upstart job, checks USB-accessible disable flags, and attempts launch once per boot. This path has host coverage but has not been validated through a physical reboot.

The Python preview exposes fixed display routes. The setup server separately exposes a page and an empty download with a fixed dialog header. Neither serves the repository as a file directory. The setup server defaults to loopback; bind it to the local network only while using the Kindle launcher.
