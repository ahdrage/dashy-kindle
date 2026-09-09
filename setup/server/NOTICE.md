# WinterBreak2 setup server attribution

This server is adapted from [KindleModding/Winterbreak2](https://github.com/KindleModding/Winterbreak2), inspected at commit `82167878e27229789a0cef50c85d9e8d897cc7c6`.

The upstream source credits **Scam.Net** with discovery and **Penguins184** with the server implementation. Its `package.json` declares the **ISC** license. Those credits and that declaration are retained. The local adaptation removes external fonts, serves one fixed local placeholder, selects a fixed unlock or launch dialog, and adds verification tests.

The device dialog files in `../device/` are derived from the same WinterBreak2 approach, with local model-checked entry points. The upstream jailbreak bootstrap is downloaded separately and is not relicensed or vendored here; the preparation helper retains its installer body unchanged.
