# Repository guidance

## Purpose and status

Channel Blue is an AGPL-3.0-or-later Bluesky client for the Nintendo Wii. It is a real, linked MVP candidate rather than a desktop mock: libogc owns video, GX, input, networking, and SD access, while the sibling `../wolfram` C SDK owns AT Protocol/XRPC, authentication, and Wii HTTPS. The DOL boots in Dolphin, but the README correctly leaves real-hardware login, TLS, session refresh, and live social flows unverified. Do not turn build or emulator success into a hardware-compatibility claim.

Read `README.md`, `Makefile`, and the relevant public header before changing a module. Protocol work must also be checked against `../wolfram`; wire-format questions should be checked against the local upstream checkout at `../../Local/atproto`, not guessed from generated names.

## Repository map

- `source/main.c`: libogc/GX startup, SD mounting, controller construction, the network-initialization worker, entropy provisioning, and the single frame loop.
- `source/navigation/`: four-tab screen stacks, rendering dispatch, and synchronous input-to-controller actions.
- `source/app/`: bounded UI/domain controllers for auth, timelines, threads, compose, discovery, input, UTF-8, avatars/media, retry, and session/entropy files. These modules are intentionally host-testable where possible.
- `source/integration/wolfram_backend.*`: the only application adapter to Wolfram. It converts owned SDK results into Channel Blue models and supplies controller callbacks.
- `source/render/` and `source/components/`: GX state, embedded Inter/FreeType glyph rendering, JPEG/PNG decoding, the eight-slot decoded-texture LRU, and navigation chrome.
- `tests/`: host-side controller, persistence, UTF-8, retry, input, and Wolfram-adapter tests. These do not exercise libogc, GX, IOS networking, or real SD behavior.
- `data/`, `icon.png`, `meta.xml`: embedded/deployment assets. `dist/`, `build/`, `channel-blue.dol`, and generated `*_bin.*` files are build products.

## Architecture and invariants

- Rendering, input polling, navigation, and XRPC-backed actions all run on the main loop. The sole LWP worker performs startup networking/entropy initialization; there is no render thread or message-queue architecture. Network calls made from navigation are blocking, so do not introduce races by assuming otherwise.
- Keep the layer boundary `libogc/Wii platform -> Wolfram transport and agent -> integration adapter -> app controllers -> navigation/rendering`. Do not hand-roll XRPC, auth, signing, TLS, or lexicon parsing in this repository.
- Wolfram result objects and Channel Blue models have different ownership. Conversion code must duplicate required strings, free partial results on every failure path, and use the matching `wf_*_free` or `cb_*_free` routine. Preserve bounded capacities and response limits; the Wii has no virtual memory.
- Social actions depend on exact identifiers: replies snapshot the selected post's URI/CID and root URI when compose opens; follow takes the author's DID; notification post navigation uses `reasonSubject`; like/repost deletion uses the viewer record URI. Do not substitute display handles or the currently selected row later.
- The input and renderer are UTF-8-aware. `cb_utf8_*` edits by scalar boundary, and `font.c` decodes code points into a bounded 192-entry FreeType glyph cache with replacement-glyph fallback. Preserve byte and code-point limits together.
- GX state is global. Text and image paths switch vertex/texture state and restore the solid-quad format afterward. Texture buffers are 32-byte aligned and cache-flushed. Preserve GX tile layout, aligned dimensions, and explicit ownership when touching render code.
- Images are decoded from untrusted network bytes. The backend accepts HTTPS URLs only and caps compressed responses at 4 MiB; PNG source dimensions/pixels and decoded output sizes are bounded; decoded thumbnails share an eight-slot LRU intended to stay below roughly 512 KiB. Maintain all checks before allocation or expansion. URL hashing is a cache key, not a trust boundary.

## Credentials, entropy, and persistence

- Login passwords remain only in the in-memory form and are cleared after successful login. The saved session is different: `sd:/apps/channel-blue/session.dat` contains service, access JWT, refresh JWT, handle, and DID in plaintext. It uses a bounded newline format and temp-file rename, but no encryption, permission hardening, or durability `fsync`. Treat the SD card as sensitive and never log or commit these values.
- `sd:/apps/channel-blue/entropy.bin` is a unique 64-byte seed, not a distributable default. Startup loads it into Wolfram, rotates it before TLS use, saves the replacement, and only then commits Wolfram's pending seed. Do not weaken the load/rotate/save/commit ordering or silently fall back to a shared seed.
- `make bundle` creates a fresh entropy file under `dist/`; the whole bundle must remain unique per installation. Never commit `dist/`, `entropy.bin`, tokens, passwords, private keys, or captured authenticated traffic.
- Logout deletes the persisted session and clears account-scoped controllers and decoded textures. Any new cache holding account data must join that cleanup path.

## Build and verification

The build expects devkitPro/devkitPPC plus Wii portlibs and a sibling Wolfram checkout. Wolfram's Wii script builds its repo-local mbedTLS/CA-backed transport; do not describe mbedTLS as an ordinary preinstalled portlib. `WII_PORTLIBS` can override the default portlibs prefix.

- `make test`: build and run host tests for pure app modules and the Wolfram adapter, including a host Wolfram build.
- `make`: cross-build and link `channel-blue.dol`.
- `make verify`: run host tests, a parallel cross-build, and a tracked-diff check. This is the normal pre-commit validation.
- `make bundle`: create the Homebrew layout and a new per-installation entropy seed under `dist/`.
- `make dolphin`: launch the locally configured Dolphin setup. It is interactive and may write emulator state.
- `make release`: copy a bundle outside the repository to `/Volumes/Storage/Wii software`; this is a deployment side effect, not a validation command. Do not run it without explicit intent.
- `make clean`: remove local build products.

Run the narrow host test while iterating, then `make verify` when the installed toolchains permit it. Hardware-sensitive changes additionally need a Wii check: TLS/CA validation, entropy persistence across boots, SD rename behavior, Wi-Fi timeouts, keyboard/controller input, and GX rendering cannot be certified by host tests. Dolphin is useful for UI and boot smoke tests but is not evidence of real IOS network behavior.

## Change discipline

- Match surrounding C style; the tree mixes tabs in app code with spaces in render code, so do not mechanically reformat unrelated lines. Keep public ownership/lifetime comments accurate.
- Add or update a host test for controller logic, bounds, parsing, UTF-8, persistence, retries, and adapter conversion. Keep libogc-dependent code out of host units unless an explicit seam is added.
- Preserve bounded retry behavior and distinguish transient network failures from auth, protocol, allocation, and invalid-input failures. Never fabricate success for an incomplete path.
- Prefer typed `wf_bsky_agent_*` APIs. If the required SDK surface is missing, implement and test it in Wolfram first rather than bypassing the boundary here.
- Keep commits atomic and conventional, and do not add AI co-author trailers. Do not stage generated bundles, DOLs, secrets, emulator state, or unrelated worktree changes.

## Current high-risk gaps

The live Wii path remains the authority. Before calling the MVP complete, verify initial entropy provisioning, HTTPS certificate validation, login and refresh persistence, timeline pagination, post/reply, like/repost deletion, DID-based follow/unfollow, search, notifications and seen-state, profiles, thread traversal, avatars/media, logout cleanup, and non-ASCII input/rendering against a real PDS on hardware. Blocking network and eager avatar/media prefetch currently occur in input/navigation paths, so performance work should start there without inventing concurrency that the ownership model cannot support.
