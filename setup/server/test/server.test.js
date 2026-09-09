import assert from "node:assert/strict";
import { once } from "node:events";
import { test } from "node:test";
import { createApp } from "../api/index.js";

async function withServer(mode, check) {
    const server = createApp({ mode }).listen(0, "127.0.0.1");
    await once(server, "listening");
    try {
        await check(`http://127.0.0.1:${server.address().port}`);
    } finally {
        server.closeAllConnections();
        await new Promise(resolve => server.close(resolve));
    }
}

for (const [mode, dialog, label] of [
    ["launch", "dialoger-launch", "Open Dashy"],
    ["unlock", "dialoger-pw1-retry", "Unlock and install Dashy"],
]) {
    test(`${mode} serves its fixed local dialog and empty download`, async () => {
        await withServer(mode, async base => {
            const page = await fetch(base);
            assert.match(await page.text(), new RegExp(label));
            const result = await fetch(`${base}/download?file=/etc/passwd`);
            assert.equal(result.status, 200);
            assert.equal(result.headers.get("cache-control"), "no-store");
            assert.match(result.headers.get("content-type"), /application\/x-mobipocket-ebook/);
            assert.ok(result.headers.get("content-disposition").includes(`/mnt/us/winterbreak2/${dialog}`));
            assert.equal((await result.arrayBuffer()).byteLength, 0);
            assert.equal((await fetch(`${base}/package.json`)).status, 404);
            assert.equal((await fetch(`${base}/assets/placeholder.mobi`)).status, 404);
        });
    });
}

test("rejects an unknown mode before serving anything", () => {
    assert.throws(() => createApp({ mode: "arbitrary-command" }), /mode/);
});
