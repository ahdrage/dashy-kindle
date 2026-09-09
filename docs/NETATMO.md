# Netatmo on the Kindle

The native app connects directly to Netatmo over verified HTTPS. It reads indoor temperature, CO2 and outdoor temperature every **three minutes**, matching the [original Dashy flow](https://github.com/ahdrage/Dashy-Home-Dashboard-for-Waveshare-ESP32-S3-RLCD-4.2/blob/8252b6c881a326facea394f824c0a6aa1007d7e5/docs/netatmo-api.md).

The clock still updates once per minute. Network work runs separately so a slow request does not block the clock or exit handling. No Mac server is needed for ongoing data fetching once the app is open.

## 1. Prepare private credentials

Create a Netatmo app and authorize it with **`read_station`**. Use the client ID, client secret and initial refresh token from [Netatmo's OAuth setup](https://dev.netatmo.com/apidocumentation/oauth). The original project's guide above includes authorization-code exchange steps.

From the repository root, create a private file using the example:

```sh
mkdir -p secrets
cp setup/netatmo.example.json secrets/netatmo.json
chmod 600 secrets/netatmo.json
```

Fill in `client_id`, `client_secret`, and `refresh_token`. Leave `station_id` empty to use the first station, as the original firmware does. Set a specific station ID to select one explicitly; an unknown ID shows an error rather than silently choosing another station.

Do not overwrite an existing working credential file with the blank example. The `secrets/` directory and runtime credential filenames are ignored by Git. Credentials are not compiled into the binary and must not be placed in the app's font/TLS asset directory or a public archive.

Use a separate authorization for each independently running device. Once transferred, the Kindle maintains its own changing refresh token; an older copy on the Mac may stop working.

## 2. Verify the connection before transfer

Install the [native build dependencies](../native/README.md), then run:

```sh
.venv/bin/python tools/check_netatmo.py
```

This builds a host checker using the **same C++ OAuth client, BearSSL HTTPS transport, and atomic token store** used by the Kindle. It fetches real station readings and prints only the readings and a connection result. It saves any replacement refresh token back into the private credential file.

The check intentionally changes the refresh-token file. Run it before transferring credentials. After the Kindle starts maintaining its own token, do not repeatedly test an old Mac copy. To build the checker without contacting Netatmo, use `--build-only`.

The live account check succeeded on the host, and the user subsequently confirmed live Netatmo readings on the physical Kindle on 9 September 2026.

## 3. Stage credentials separately from the app

Build/package the new application and copy its [USB files](../native/INSTALL.md#copy-the-bundle). While the Kindle is mounted in USB Drive Mode, run:

```sh
.venv/bin/python tools/stage_netatmo.py --kindle /Volumes/Kindle
```

Use the actual mount path if it differs. The helper validates the private file and expected firmware before writing `dashy/netatmo.pending.json`. It never prints credential values.

Safely eject and open Dashy with the existing browser launcher. The updater closes any old session, installs the new app, and imports the staged credentials. It verifies the copy, makes it accessible only to the app's user, then removes the USB staging copy.

On the Kindle the current credentials live at:

```text
/var/local/kinduino/sketches/dashy/files/netatmo.json
```

The app runs as uid 99 and writes this file with mode 0600. It persists rotated refresh tokens through a synchronized temporary file and atomic rename. This directory survives application updates. Normal updates should not include a new pending credential file.

Keep the Kindle connected to Wi-Fi through its normal settings. The native runtime uses that saved connection; no Wi-Fi SSID or password is added to this configuration.

## What the display means

| Message | Meaning |
| --- | --- |
| IKKE TILKOBLET | Valid private credentials have not been imported |
| HENTER MÅLINGER | Connecting for the first reading |
| HENTET … | Time of the last successful API fetch |
| NETTVERKSFEIL | Connection failed; any existing readings are retained |
| ELDRE MÅLINGER | Station/module is unreachable, or its measurement is more than 30 minutes old |
| KOBLE TIL PÅ NYTT | The grant was rejected; stage fresh authorization and reopen |
| VENTER PÅ NETATMO | Rate limited; the next request is delayed |
| MANGLER MÅLINGER | Station selection or returned data is missing/invalid |
| KUNNE IKKE LAGRE | The replacement token could not be saved; check device storage |
| STILL KLOKKEN | The Kindle time is unset; correct it before HTTPS can work |

A failed fetch never replaces measurements with dummy values. Missing individual measurements show placeholders. A restart starts with placeholders until the first successful fetch; readings themselves are not cached to disk. `HENTET` is the fetch time, not a claim that every sensor measured at that exact moment. When provided, sensor timestamps and reachability determine the older-measurement warning.

## OAuth, selection and failures

The client posts URL-encoded form fields to `https://api.netatmo.com/oauth2/token`, then sends the access token as a Bearer header to `https://api.netatmo.com/api/getstationsdata`. Tokens never go in URL query strings.

It refreshes shortly before expiry and retries a station request once after 401/403. A rejected grant stops further requests until credentials are replaced and the app restarted. A 429 response delays polling according to numeric Retry-After, bounded between three minutes and one hour. Other failures retry on the normal three-minute schedule.

The first station supplies indoor Temperature and CO2. The outdoor selection prefers a `NAModule1`; if its temperature is absent, it uses the first module's temperature, matching the original Dashy implementation. Numeric types and ranges are checked; booleans, numeric strings, missing values and malformed JSON are not displayed as valid measurements.

## TLS and tests

The app verifies Netatmo's certificate chain, hostname and validity dates using the [checked public root](../assets/certs/README.md). It never falls back to insecure TLS. The Kindle clock must be accurate. A future change of issuing certificate authority may require an asset update.

```sh
sh tools/test_netatmo.sh
sh tools/test_native.sh
.venv/bin/python -m unittest discover -s tests -v
```

Checks cover URL encoding, token rotation and restart, expiry, bounded authorization retry, rate limits, invalid responses, missing data, private atomic storage, credential transfer, pixel rotation and partial refreshes. A worker test deliberately blocks a network request and verifies that display reads remain responsive.

The Python browser/PDF preview continues to use sample data. Calendar integration, automatic clock synchronization, battery optimization and unattended startup validation remain separate work.

## Confirmed device build

- Date: 9 September 2026
- Hardware: PW1 Wi-Fi (B024), firmware 5.6.1.1
- Orientation: landscape, USB/power edge left
- Build ID: `dashy-netatmo-1d8a274b4326b4c4`
- Program SHA-256: `e4143174ef98389a698a8c24afb4b4a4eff7eceface9c561d3ab8bfad8d63130`
- Result: user confirmed live readings with the Netatmo footer after USB staging and browser launch.

This confirms the live-data path; it does not establish long-term battery life or unattended boot reliability. Future generated bundles still require their own device confirmation.
