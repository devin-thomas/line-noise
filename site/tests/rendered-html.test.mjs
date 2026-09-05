import assert from "node:assert/strict";
import { readFile, stat } from "node:fs/promises";
import test from "node:test";

async function render(path = "/") {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);

  return worker.fetch(
    new Request(`http://localhost${path}`, {
      headers: { accept: "text/html" },
    }),
    {
      ASSETS: {
        fetch: async () => new Response("Not found", { status: 404 }),
      },
    },
    {
      waitUntil() {},
      passThroughOnException() {},
    },
  );
}

test("server-renders the complete LINE NOISE showcase", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  assert.match(response.headers.get("content-type") ?? "", /^text\/html\b/i);

  const html = await response.text();
  assert.match(html, /<title>LINE NOISE! \| Playable GBA Vertical Slice<\/title>/i);
  assert.match(html, /PLAYABLE GAME BOY ADVANCE VERTICAL SLICE/);
  assert.match(html, /Connections carry people\./);
  assert.match(html, /Crossings carry information\./);
  assert.match(html, /A real ROM, not a target render\./);
  assert.match(html, /href="\/downloads\/line-noise\.gba"/);
  assert.match(html, /src="\/screens\/four-live-cords\.png"/);
  assert.match(html, /DESIGN DIAGRAM \/ NOT GAMEPLAY/);
  assert.doesNotMatch(html, /Your site is taking shape|SkeletonPreview/);
});

test("ships the verified cartridge and native evidence", async () => {
  const romUrl = new URL("../public/downloads/line-noise.gba", import.meta.url);
  const rom = await readFile(romUrl);
  assert.equal(rom.length, 192920);
  assert.equal(rom.subarray(0xa0, 0xac).toString("ascii").replace(/\0+$/, ""), "LINE NOISE!");
  assert.equal(rom.subarray(0xac, 0xb0).toString("ascii"), "LNSE");

  const assets = [
    "../public/screens/title.png",
    "../public/screens/four-live-cords.png",
    "../public/screens/shift-brief.png",
    "../public/screens/city-memory.png",
    "../public/screens/epilogue.png",
    "../public/design/mechanic-diagram.png",
    "../public/brand/uppercut-labs-logo.png",
  ];
  for (const asset of assets) {
    assert.ok((await stat(new URL(asset, import.meta.url))).size > 100);
  }
});

test("keeps native captures crisp when the layout scales them", async () => {
  const css = await readFile(new URL("../app/globals.css", import.meta.url), "utf8");
  assert.match(css, /\.pixel-screen,[\s\S]*\.capture-frame img,[\s\S]*\.mini-screen img/);
  assert.match(css, /image-rendering:\s*pixelated/);
  assert.match(css, /image-rendering:\s*crisp-edges/);
});
