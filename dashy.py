"""Dashy's clock and station readings, rendered for a first-generation Paperwhite."""

import argparse
import io
import json
import math
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from zoneinfo import ZoneInfo

from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen import canvas

ROOT = Path(__file__).resolve().parent
OSLO = ZoneInfo("Europe/Oslo")
SIZES = {"portrait": (758, 1024), "landscape": (1024, 758)}
DAYS = ("Mandag", "Tirsdag", "Onsdag", "Torsdag", "Fredag", "Lørdag", "Søndag")
MONTHS = ("januar", "februar", "mars", "april", "mai", "juni", "juli",
          "august", "september", "oktober", "november", "desember")


def readings_from_netatmo(payload):
    """Match Dashy's first-station mapping, prioritizing the NAModule1 outdoor sensor."""
    if not isinstance(payload, dict):
        raise ValueError("Station data must be a JSON object")
    devices = payload.get("body", {}).get("devices", [])
    station = devices[0] if devices else {}
    indoor = station.get("dashboard_data", {})
    modules = station.get("modules", [])
    outdoor = next((module for module in modules if module.get("type") == "NAModule1"), {})
    temperature = outdoor.get("dashboard_data", {}).get("Temperature")
    if temperature is None and modules:
        temperature = modules[0].get("dashboard_data", {}).get("Temperature")

    def number(value):
        return value if type(value) in (int, float) and math.isfinite(value) else None

    co2 = number(indoor.get("CO2"))
    return {
        "indoor_c": number(indoor.get("Temperature")),
        "outdoor_c": number(temperature),
        "co2_ppm": int(co2) if co2 is not None and co2 >= 0 else None,
    }


def snapshot(payload, now):
    """Format one consistent frame; demo sensor values are never presented as live."""
    if now.utcoffset() is None:
        raise ValueError("Timestamp must include a timezone")
    local = now.astimezone(OSLO)
    readings = readings_from_netatmo(payload)

    def temperature(value):
        return "--.-°C" if value is None else f"{value:.1f}°C"

    co2 = readings["co2_ppm"]
    return {
        "time": local.strftime("%H:%M"),
        "date": f"{DAYS[local.weekday()]} {local.day}. {MONTHS[local.month - 1]} {local.year}",
        "indoor": temperature(readings["indoor_c"]),
        "outdoor": temperature(readings["outdoor_c"]),
        "co2": "---- ppm" if co2 is None else f"{co2} ppm",
        "mode": "demo",
        "generated_at": local.isoformat(timespec="seconds"),
    }


def render_png(data, orientation="portrait"):
    """Render grayscale PNG pixels directly; the Kindle needs no web fonts or JS."""
    if orientation not in SIZES:
        raise ValueError("Orientation must be portrait or landscape")
    width, height = SIZES[orientation]
    # Work on a 400-unit-wide canvas, matching the reference firmware's columns.
    scale = width / 400
    image = Image.new("L", (width, height), 255)
    draw = ImageDraw.Draw(image)

    def label(text, x, y, size, weight=400, color=0, max_width=360):
        font = ImageFont.truetype(str(ROOT / "assets/fonts/Montserrat.ttf"), round(size * scale))
        font.set_variation_by_axes([weight])
        if draw.textlength(text, font=font) > max_width * scale:
            fitted_size = max(8, int(font.size * max_width * scale / draw.textlength(text, font=font)))
            font = ImageFont.truetype(str(ROOT / "assets/fonts/Montserrat.ttf"), fitted_size)
            font.set_variation_by_axes([weight])
        draw.text((round(x * scale), round(y * scale)), text, font=font, fill=color, anchor="mt")

    def line(coords, color=85, weight=1):
        draw.line(tuple(round(value * scale) for value in coords), fill=color, width=max(1, round(weight * scale)))

    if orientation == "portrait":
        clock_y, clock_size, date_y = 92, 108, 212
        separator_y, title_y, value_y, divider_bottom = 277, 308, 347, 387
        footer_y, accent_y = 487, 516
    else:
        clock_y, clock_size, date_y = 30, 78, 105
        separator_y, title_y, value_y, divider_bottom = 141, 163, 194, 229
        footer_y, accent_y = 255, 280

    label(data["time"], 200, clock_y, clock_size, weight=400)
    label(data["date"], 200, date_y, 19, weight=500)
    line((30, separator_y, 370, separator_y))
    for center, title, value in ((67, "INNE", data["indoor"]),
                                 (200, "CO2", data["co2"]),
                                 (333, "UTE", data["outdoor"])):
        label(title, center, title_y, 14, weight=500, color=68)
        label(value, center, value_y, 29, weight=500, max_width=117)
    for x in (133, 267):
        line((x, title_y + 1, x, divider_bottom))
    label("DEMOVISNING · EKSEMPELDATA", 200, footer_y, 9, weight=500, color=85)
    line((170, accent_y, 230, accent_y), color=0, weight=3)
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=True)
    return output.getvalue()


def render_pdf(data):
    """Create a static two-page preview readable by the stock Kindle PDF reader."""
    output = io.BytesIO()
    pdf = canvas.Canvas(output, pagesize=SIZES["portrait"], pageCompression=1)
    pdf.setTitle("Dashy - Demo")
    pdf.setAuthor("Dashy for Kindle")
    pdf.setSubject("Sample sensor readings. Static preview; the clock in this PDF does not update.")
    for orientation, (width, height) in SIZES.items():
        pdf.setPageSize((width, height))
        png = ImageReader(io.BytesIO(render_png(data, orientation)))
        pdf.drawImage(png, 0, 0, width=width, height=height)
        pdf.showPage()
    pdf.save()
    return output.getvalue()


def make_server(host, port, data_path):
    """Serve only the explicit display routes; never serve the project directory."""
    data_path = Path(data_path)

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            url = urlsplit(self.path)
            orientation = parse_qs(url.query).get("orientation", ["portrait"])[0]
            if url.path not in ("/", "/display", "/frame.png", "/api/dashboard", "/dashboard.pdf"):
                self.send_error(404)
                return
            if orientation not in SIZES:
                self.send_error(400, "Unknown orientation")
                return
            try:
                now = datetime.now(OSLO)
                data = snapshot(json.loads(data_path.read_text()), now)
                if url.path == "/frame.png":
                    body, content_type = render_png(data, orientation), "image/png"
                elif url.path == "/dashboard.pdf":
                    body, content_type = render_pdf(data), "application/pdf"
                elif url.path == "/api/dashboard":
                    body, content_type = json.dumps(data, ensure_ascii=False).encode(), "application/json; charset=utf-8"
                else:
                    template = "display.html" if url.path == "/display" else "preview.html"
                    html = (ROOT / "web" / template).read_text()
                    replacements = {
                        "orientation": orientation,
                        "refresh": str(60 - now.second),
                        "portrait_selected": "true" if orientation == "portrait" else "false",
                        "landscape_selected": "true" if orientation == "landscape" else "false",
                    }
                    for key, value in replacements.items():
                        html = html.replace("{{" + key + "}}", value)
                    body, content_type = html.encode(), "text/html; charset=utf-8"
            except (OSError, ValueError, TypeError, AttributeError, KeyError):
                self.send_error(503, "Demo data unavailable")
                return
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store, max-age=0")
            self.send_header("X-Content-Type-Options", "nosniff")
            if url.path == "/dashboard.pdf":
                self.send_header("Content-Disposition", 'attachment; filename="Dashy-Demo.pdf"')
            self.end_headers()
            self.wfile.write(body)

    return ThreadingHTTPServer((host, port), Handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=ROOT / "sample-data.json")
    commands = parser.add_subparsers(dest="command", required=True)
    render = commands.add_parser("render", help="Export both PNG layouts and a static PDF")
    render.add_argument("--output", type=Path, default=ROOT / "output")
    render.add_argument("--at", help="Optional ISO timestamp including timezone, for a reproducible preview")
    serve = commands.add_parser("serve", help="Start the live clock with demo station readings")
    serve.add_argument("--host", default="127.0.0.1")
    serve.add_argument("--port", type=int, default=8787)
    args = parser.parse_args()
    if args.command == "render":
        now = datetime.fromisoformat(args.at) if args.at else datetime.now(OSLO)
        data = snapshot(json.loads(args.data.read_text()), now)
        args.output.mkdir(parents=True, exist_ok=True)
        for orientation in SIZES:
            (args.output / f"dashy-{orientation}.png").write_bytes(render_png(data, orientation))
        pdf_dir = args.output / "pdf"
        pdf_dir.mkdir(exist_ok=True)
        (pdf_dir / "Dashy-Demo.pdf").write_bytes(render_pdf(data))
        print(f"Created portrait and landscape previews in {args.output.resolve()}")
    else:
        server = make_server(args.host, args.port, args.data)
        print(f"Dashy demo: http://{args.host}:{args.port}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()


if __name__ == "__main__":
    main()
