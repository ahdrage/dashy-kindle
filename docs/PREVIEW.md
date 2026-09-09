# Browser preview and static PDF

The Python version is useful for trying the design without unlocking a Kindle. It is separate from the native application.

Python 3.9+ and the system Europe/Oslo timezone database are required. Run from the repository root:

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python dashy.py serve
```

Open [the preview](http://127.0.0.1:8787) and choose **Stående** or **Liggende**. The clock refreshes at the next minute boundary. To serve it on the same Wi-Fi as a Kindle:

```sh
.venv/bin/python dashy.py serve --host 0.0.0.0
```

Use `http://HOST_IP:8787/display?orientation=landscape`, replacing `HOST_IP` with the computer's local address. The computer must remain awake for this version. Continuous browser refresh and the unmodified Kindle's sleep behavior are not confirmed on the physical unit; the native app is the verified display route.

| Route | Output |
| --- | --- |
| `/` | Preview with orientation controls |
| `/display` | Minimal automatically refreshing image page |
| `/frame.png` | Grayscale frame |
| `/api/dashboard` | Demo readings and frame timestamp |
| `/dashboard.pdf` | Static PDF containing portrait and landscape pages |

Image routes support `?orientation=portrait` or `?orientation=landscape`. The PDF always contains both orientations. Responses disable caching. The server does not expose arbitrary files or a directory listing.

## Export a static preview

```sh
.venv/bin/python dashy.py render --at 2026-09-09T16:32:00+02:00
```

Outputs are `output/dashy-portrait.png`, `output/dashy-landscape.png`, and `output/pdf/Dashy-Demo.pdf`. Copy the PDF into the Kindle's `documents` directory, safely eject, and open it from the library. It is a frozen preview: its clock and readings do not update. No jailbreak is needed for the PDF.

## Sample data

`sample-data.json` uses the shape of a Netatmo station response. The mapper uses the first station, its indoor temperature and CO2, and prefers an outdoor `NAModule1`. It falls back to another module temperature if necessary and shows placeholders for missing values. Python rereads the file on each request. Native values are generated at build time.

No OAuth, live API fetching, calendar integration or account credentials are implemented.

## Checks

```sh
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/python -m unittest discover -s tests -v
```

The suite covers field mapping, missing values, Oslo time and date rollover, image dimensions, PDF pages, HTTP delivery, and isolated device-control behavior. Optional unlock tests skip until the pinned upstream payload has been prepared as described in [setup](../setup/README.md).
