import io
import json
import threading
import unittest
from datetime import datetime, timezone
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import urlopen

from PIL import Image
from pypdf import PdfReader

import dashy

SAMPLE_PATH = Path(__file__).resolve().parents[1] / "sample-data.json"
PAYLOAD = json.loads(SAMPLE_PATH.read_text())
NOW = datetime(2026, 9, 9, 14, 32, tzinfo=timezone.utc)


class DashboardTests(unittest.TestCase):
    def test_selects_the_outdoor_module_not_first_temperature(self):
        self.assertEqual(dashy.readings_from_netatmo(PAYLOAD), {
            "indoor_c": 21.3, "outdoor_c": 14.2, "co2_ppm": 623,
        })

    def test_missing_module_uses_upstream_temperature_fallback(self):
        payload = {"body": {"devices": [{"modules": [
            {"type": "NAModule4", "dashboard_data": {"Temperature": -8.2}}
        ]}]}}
        self.assertEqual(dashy.readings_from_netatmo(payload).get("outdoor_c"), -8.2)

    def test_oslo_time_and_norwegian_date_do_not_depend_on_system_locale(self):
        data = dashy.snapshot(PAYLOAD, NOW)
        self.assertEqual(data.get("time"), "16:32")
        self.assertEqual(data.get("date"), "Onsdag 9. september 2026")
        self.assertEqual(data.get("indoor"), "21.3°C")
        self.assertEqual(data.get("co2"), "623 ppm")
        self.assertEqual(data.get("mode"), "demo")

    def test_oslo_winter_time_and_date_rollover(self):
        data = dashy.snapshot(PAYLOAD, datetime(2026, 12, 31, 23, 5, tzinfo=timezone.utc))
        self.assertEqual(data.get("time"), "00:05")
        self.assertEqual(data.get("date"), "Fredag 1. januar 2027")

    def test_missing_and_invalid_readings_are_not_displayed_as_real_values(self):
        payload = {"body": {"devices": [{"dashboard_data": {
            "Temperature": float("nan"), "CO2": -1,
        }}]}}
        data = dashy.snapshot(payload, NOW)
        self.assertEqual(data.get("indoor"), "--.-°C")
        self.assertEqual(data.get("outdoor"), "--.-°C")
        self.assertEqual(data.get("co2"), "---- ppm")

    def test_png_matches_kindle_screen_in_both_orientations(self):
        for orientation, size in [("portrait", (758, 1024)), ("landscape", (1024, 758))]:
            with self.subTest(orientation=orientation):
                content = dashy.render_png(dashy.snapshot(PAYLOAD, NOW), orientation)
                self.assertTrue(content.startswith(b"\x89PNG"), "Expected a PNG image")
                image = Image.open(io.BytesIO(content))
                self.assertEqual(image.size, size)
                self.assertEqual(image.mode, "L")
                self.assertGreater(len(image.getcolors(256)), 2)

    def test_pdf_has_portrait_and_landscape_pages_for_stock_kindle(self):
        content = dashy.render_pdf(dashy.snapshot(PAYLOAD, NOW))
        self.assertTrue(content.startswith(b"%PDF"), "Expected a PDF document")
        reader = PdfReader(io.BytesIO(content))
        self.assertEqual(len(reader.pages), 2)
        self.assertEqual(str(reader.metadata.title), "Dashy - Demo")
        for page, size in zip(reader.pages, [(758, 1024), (1024, 758)]):
            self.assertEqual((float(page.mediabox.width), float(page.mediabox.height)), size)

    def test_rejects_unknown_orientation(self):
        with self.assertRaises(ValueError):
            dashy.render_png(dashy.snapshot(PAYLOAD, NOW), "sideways")

    def test_rejects_ambiguous_timestamp(self):
        with self.assertRaises(ValueError):
            dashy.snapshot(PAYLOAD, datetime(2026, 9, 9, 14, 32))

    def test_http_serves_uncached_images_without_exposing_project_files(self):
        server = dashy.make_server("127.0.0.1", 0, SAMPLE_PATH)
        self.assertIsInstance(server, ThreadingHTTPServer)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = "http://127.0.0.1:%d" % server.server_port
        try:
            with urlopen(base + "/frame.png?orientation=landscape") as response:
                self.assertIn("no-store", response.headers["Cache-Control"])
                self.assertEqual(response.headers["Content-Type"], "image/png")
                self.assertEqual(Image.open(io.BytesIO(response.read())).size, (1024, 758))
            with urlopen(base + "/display") as response:
                html = response.read().decode()
                self.assertIn('http-equiv="refresh"', html)
                self.assertNotIn("<script", html)
            with self.assertRaises(HTTPError) as error:
                urlopen(base + "/.env")
            self.assertEqual(error.exception.code, 404)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    unittest.main()
