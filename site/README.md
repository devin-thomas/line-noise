# LINE NOISE! Showcase

The artifact-first showcase for the playable Game Boy Advance vertical slice.
It uses only native mGBA captures and supplied LINE NOISE design assets.

Live at [line-noise.uppercut-labs.workers.dev](https://line-noise.uppercut-labs.workers.dev).

## Run locally

```bash
cd ..
DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make -C game
cd site
npm ci
npm run dev
```

## Validate

```bash
npm run lint
npm test
```

The build test verifies the server-rendered page, required artwork, ROM size,
and cartridge header. Every site build synchronizes the downloadable ROM and
canonical captures from the validated game build.

## Deploy

The showcase deploys directly to the `line-noise` Cloudflare Worker in the
Uppercut Labs account. It does not use ChatGPT Sites.

```bash
npm run typegen
npm run deploy:dry-run
npm run deploy
```
