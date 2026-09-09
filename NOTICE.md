# Attribution

This project adapts the display design and Netatmo field mapping of:

- **Dashy**, copyright (c) 2026 dashy contributors, MIT licensed.
- Source: https://github.com/ahdrage/Dashy-Home-Dashboard-for-Waveshare-ESP32-S3-RLCD-4.2
- Inspected revision: `8252b6c881a326facea394f824c0a6aa1007d7e5`.
- Reference files: `docs/current-screen.md`, `docs/netatmo-api.md`, `mockup.html`, and the `setup()` layout in `firmware/arduino/08_LVGL_V8_Test/08_LVGL_V8_Test.ino`.

The ESP32, Waveshare, and LVGL drivers have not been copied into this project. The Python preview and native C++ renderer are separate Kindle implementations.

**Montserrat** is bundled under the SIL Open Font License. Copyright and full license are in `assets/fonts/OFL.txt`. Source: https://github.com/google/fonts/tree/main/ofl/montserrat

The native build instantiates Montserrat at weights 400 and 500 and renames the derived static fonts **Dashy Sans**. They retain the original OFL notice.

**Kinduino 0.5.2**, copyright (c) 2026 kinduino contributors, supplies the native SDK and runtime. Source: https://github.com/best-effort-labs/kinduino/tree/v0.5.2; commit `f77ef002faf1cae520db4038d9c4ee4a3c01fef4`. The SDK is MIT licensed. The unmodified device package includes separately licensed FBInk components; its GPLv3 license and original source offer are retained in `USB-ROOT/extensions/kinduino/`.

The native archive includes notices for linked components: stb_truetype (Sean Barrett), Adafruit GFX, musl, libc++, libc++abi, libunwind, and Zig. Their licenses are included in the archive's `licenses/` directory. Kinduino's graphics service is a separate process; the Dashy executable uses its display protocol and does not link FBInk.

The local setup server and device dialog files are adapted from **WinterBreak2**, credited to Scam.Net and Penguins184. The upstream server package declares ISC. See [setup/server/NOTICE.md](setup/server/NOTICE.md) for the pinned source and adaptation details. The upstream jailbreak bootstrap is downloaded separately and is not included in this repository.

The Netatmo implementation follows the original Dashy OAuth flow and field mapping, with certificate verification, private persistent token rotation, bounded retries and a background worker. **ArduinoJson 7.4.3** (Benoit Blanchon, MIT) and **BearSSL** (Thomas Pornin, MIT) come from the pinned Kinduino SDK; their notices are included in the device archive. The public DigiCert root certificate’s source and fingerprint are in [assets/certs/README.md](assets/certs/README.md).
