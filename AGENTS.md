# AGENTS.md

Agentic principles and technical context for the `channel-blue` repository.

## License

Channel Blue is licensed under the **GNU Affero General Public License v3.0 or later** (AGPL-3.0-or-later). See [LICENSE](LICENSE) for the full text. The `wolfram` SDK it links against is MIT-licensed; the AGPL's network interaction clause applies to Channel Blue's own source code.

## Project overview

Channel Blue is a Bluesky client for the Nintendo Wii. It runs as a homebrew application via the Homebrew Channel, rendering posts from the AT Protocol (XRPC) onto the Wii's 480p display. The `wolfram` C SDK (linked from `/Volumes/Storage/Developer/Git/wolfram`) provides the protocol layer; Channel Blue owns the UI, input handling, and Wii-specific hardware integration.

## Technical philosophy

1. **Wii-first, honest constraints**: the Wii has 24 MB MEM1 (1T-SRAM) and ~48 MB usable MEM2 (IOS reserves ~12–16 MB of the 64 MB GDDR3). Every allocation matters. Stream data rather than buffering entire timelines in RAM. Prefer fixed-size buffers and arena allocation over unbounded `malloc`/`free`.
2. **Protocol parity with wolfram**: consume `libwolfram` APIs (`wf_xrpc_*`, `wf_agent_*`) for all AT Protocol / XRPC work. Never hand-roll XRPC request/response parsing, OAuth, or session management — that is `wolfram`'s job. When `wolfram` adds a new typed wrapper, adopt it.
3. **No hand-rolled crypto or TLS**: TLS/HTTPS is required (Bluesky PDS endpoints are HTTPS-only). Cross-compile mbedTLS as a Wii portlib and wrap it behind a thin transport layer. Do not implement certificate validation or symmetric ciphers from scratch.
4. **GX is the renderer, not OpenGL**: the Wii's ATI Hollywood GPU is programmed through libogc's GX subsystem. Text is rendered with FreeType into textures, then drawn as GX quads. There is no shader pipeline — all geometry goes through the fixed-function vertex/texgen pipeline. Learn the GX setup sequence (FIFO allocation, viewport, projection, vertex descriptor) before writing rendering code.
5. **Threaded architecture**: Wii homebrew supports cooperative threads via `lwp` (libogc). Use a dedicated render thread for GX draw operations and a main/UI thread for input handling and XRPC calls. Communicate between threads via message queues (`LWP_MessageQueue`), not shared mutable state.
6. **Stubs are honest**: unimplemented functions return an error code and carry a `TODO` explaining what is missing — never a silent no-op or fabricated success.
7. **Ownership is explicit**: every heap-allocated output has a matching `_free` function documented next to it. No hidden allocations, no implicit ownership transfer.

## Code style

- **C99 with libogc conventions**: the devkitPPC toolchain is GCC-based (powerpc-eabi). Use C99 features (variable-length arrays, designated initializers, `stdint.h` types) but avoid C11 `_Generic` or `_Atomic` — the PPC toolchain does not support them reliably. Match libogc's naming: `u8`, `u16`, `u32`, `s8`, `s16`, `s32`, `f32`, `BOOL`.
- **Comments are allowed and encouraged** where they aid understanding — especially next to public API declarations (ownership rules, lifetime), non-obvious GX/VIDEO/WiFi details, and `TODO` notes. The existing `wolfram` codebase uses comments pervasively; match that. Do not add noise comments that merely restate the code.
- **Atomic conventional commits**: every commit must contain exactly one logical change. Scope by module — `feat(ui)`, `feat(timeline)`, `feat(gx)`, `fix(network)`, `fix(input)`, `docs(roadmap)`, etc. Never mix unrelated changes in a single commit (e.g. do not combine a code change with a docs update). Feature work lands on a dedicated `feat/<area>` branch and is merged to `main` with `--no-ff` so the branch history is preserved. If a commit touches multiple concerns, split it into multiple sequential commits.
- **No AI co-authors**: commits must not add a `Co-authored-by:` trailer crediting an AI agent. AI assistance is welcome, but credit for committed work goes to human authors only.
- **Module layering**: transport (lwip + mbedTLS) → XRPC (`wolfram`) → session/agent (`wolfram`) → UI (GX rendering + input). New features follow the existing pattern: call `wolfram` agent APIs for data, then pass results to the GX rendering layer.
- **No commented-out code** left in place; delete dead code or move it to a test.
- Follow the surrounding file's indentation and brace style (tabs for indentation, K&R braces).

## Wii homebrew specifics

### Toolchain and build system

- **Toolchain**: devkitPPC (`powerpc-eabi-gcc`), installed via devkitPro pacman.
- **SDK**: libogc — provides GX (graphics), VIDEO (display), `lwp` (threads), `wiiuse`/`WPAD_` (Wiimote), `lwip` (TCP/IP), `fat` (SD card FAT filesystem), `con` (console text via GX).
- **Build**: standard devkitPro Makefile. Output is a `.dol` (executable). `meta.xml` and `icon.png` live at the repo root and are deployed to `sd:/apps/channel-blue/` alongside the `.dol`.
- **Portlibs**: additional libraries cross-compiled for PPC via `dkp-pacman`. Needed portlibs include: `mbedtls` (TLS), `libpng` (PNG decoding for avatars/embeds), `freetype` (font rendering), `zlib` (decompression, likely a transitive dependency).
- **Deploy**: `wiiload channel-blue.dol` uploads to a running Wii over WiFi, or copy the `.dol` to the SD card manually.

### Memory model

- **MEM1** (24 MB, fast 1T-SRAM): prefer for hot data — GX command FIFO, framebuffers, texture data, frequently accessed post text.
- **MEM2** (48 MB usable, GDDR3): suitable for larger allocations — JSON parse buffers, FreeType glyph caches, TLS session state, network receive buffers.
- **No virtual memory**: the Wii has no MMU for user code. `malloc`/`free` manage a fixed heap. OOM is a real and unrecoverable crash. Always check return values and fail gracefully.
- **GX Embedded Framebuffer** (3 MB): allocated from MEM1 by `VIDEO_GetCurrentMode()->fbWidth * VIDEO_GetCurrentMode()->efbHeight * GX_FIFOIZ`. Do not overlap with application memory.

### Networking

- **lwIP** (bundled with libogc): provides BSD-style sockets (`socket`, `connect`, `send`, `recv`, `close`). Initialize via `net_init()` (calls `net_init()` from libogc's network subsystem) after calling `net_init()` once at startup. WiFi is 802.11b/g — expect ~2–4 Mbps throughput in practice.
- **TLS**: all Bluesky XRPC endpoints use HTTPS. Link `libmbedtls` and initialize an mbedTLS SSL context per XRPC call. Validate the PDS certificate chain against a bundled root CA certificate (bundled as a binary asset in the DOL or loaded from SD).
- **DNS**: lwIP provides `gethostbyname()`. Use it to resolve `bsky.social` (or whichever PDS the user configures).
- **Timeouts**: WiFi on the Wii is unreliable. Set aggressive socket timeouts (5–10 seconds) and implement retry with exponential backoff. The user may be on a weak signal or far from the access point.
- **IOS network calls**: the actual WiFi hardware is managed by IOS (the ARM co-processor). lwIP communicates with IOS via IPC. This means networking calls block the PPC thread — use a dedicated network thread or non-blocking I/O pattern.

### Graphics (GX)

- **Display mode**: 640×480 (NTSC/PAL 480i/480p). Target 480p progressive scan when available (`CONF_GetProgressive()`).
- **GX setup sequence**: `VIDEO_Init()` → `VIDEO_GetPreferredMode()` → allocate XFB → `GX_Init()` → `GX_SetViewport()` → configure vertex descriptor → set projection matrix. This must happen before any draw calls.
- **Text rendering**: use FreeType to rasterize TTF glyphs into 8-bit alpha textures, then upload to GX via `GX_InitTexObj()` and draw as textured quads with alpha blending. Pre-render a bitmap font atlas at startup to avoid per-frame FreeType calls.
- **Texture memory**: GX texture memory (TMEM) is only 4 KB on the hardware. Large textures must be tiled and loaded via `GX_LoadTexObj()`. Keep avatar thumbnails small (e.g., 32×32 or 48×48 pixels, YUV422 or IA8 format).
- **Double buffering**: use `VIDEO_SetNextFramebuffer()` / `VIDEO_Flush()` with two framebuffers for tear-free presentation.
- **Performance**: the GX command FIFO fills at ~128 KB/s. Avoid submitting more geometry than necessary each frame. For a text-heavy UI, pre-render text into display lists (`GX_BeginDispList` / `GX_EndDispList`) and replay them.

### Input

- **Wiimote** (`WPAD_`): primary input device. Buttons: D-pad (navigation), A (select/post), B (back/cancel), Plus/Minus (scroll), Home (menu). WPAD handles IR pointer and Nunchuk/Classic Controller expansion detection.
- **USB keyboard** (`kbd_*` from `libwiikeyboard`): optional — for typing post text. Highly recommended for a social media client where the user composes messages.
- **Classic Controller** (`WPAD_EXP_CLASSIC`): if detected, map face buttons and analog stick for scrolling/navigation.
- **Input polling**: call `WPAD_ScanPads()` and `PAD_ScanPads()` once per frame (in the main loop, not in the render thread). Read button states with `WPAD_ButtonsDown()`.

### File I/O

- **SD card**: mounted via `fatMountSimple("sd:", &__io_wiisd)` at startup. All persistent data (settings, cached credentials, avatar cache) lives on the SD card under `sd:/apps/channel-blue/`.
- **NAND**: not used for user data. Avoid writing to NAND — it has limited write cycles and requires IOS permissions.

## Code style — GX specifics

- GX calls are verbose and stateful. Group related GX state changes together and comment which "pass" or "stage" they belong to (e.g., `// Stage 1: Configure texture environment for text quads`).
- The GX state machine is global. Reset relevant state at the start of each frame or when switching between rendering modes (e.g., text vs. images).
- Use `GX_Begin` / `GX_End` pairs for immediate-mode drawing. Always specify the correct vertex count in `GX_Begin` to avoid FIFO corruption.

## Development workflow

- **Build**: `make` (uses the devkitPro Makefile; requires `DEVKITPPC` env var and devkitPro pacman packages installed).
- **Deploy**: `wiiload channel-blue.dol` (WiFi upload to Wii) or copy `channel-blue.dol` to `sd:/apps/channel-blue/`.
- **Test on hardware**: launch from the Homebrew Channel. There is no Wii emulator that accurately replicates GX rendering or WiFi behavior — real hardware testing is mandatory.
- **Dolphin emulator**: useful for testing GX rendering and basic input (Wiimote pointer). Does not replicate WiFi timing, IOS behavior, or SD card I/O accurately. Use for UI/layout iteration only.
- **Wolfram integration**: build `libwolfram` as a static library (`.a`) and link it into the Channel Blue DOL. Ensure `wolfram`'s `CMakeLists.txt` is built with the same devkitPPC cross-compiler. The `wolfram` repo at `/Volumes/Storage/Developer/Git/wolfram` is the source of truth for AT Protocol behavior.
- **Lexicon coverage**: cross-reference `bluesky-social/atproto` lexicons for wire formats. The `wolfram` SDK generates typed wrappers from lexicons — before writing raw XRPC calls, check if `wolfram` already provides a typed wrapper.

## Current state

The project is in early development. The repository currently contains only the git skeleton; no source code has been committed yet. The immediate next steps are:

1. Set up the devkitPro Makefile and verify a "hello world" DOL builds and runs on the Wii.
2. Cross-compile `libwolfram` for PPC and link it into the project.
3. Implement WiFi initialization (`net_init()`) and verify DNS resolution and HTTPS connectivity to `bsky.social`.
4. Set up the GX rendering pipeline (framebuffer, viewport, basic 2D quad drawing).
5. Implement a minimal XRPC client using `wolfram`'s `wf_xrpc_*` APIs over lwIP + mbedTLS.
6. Build the authentication flow (`createSession` / `refreshSession`) and persist the session to SD card.

## Next planned work

- [ ] Bootstrap: Makefile, hello-world DOL, SD card deployment
- [ ] Cross-compile mbedTLS and libwolfram for PPC
- [ ] WiFi init + HTTPS GET to bsky.social (verify TLS works)
- [ ] GX rendering pipeline: framebuffer init, 2D quad drawing, texture upload
- [ ] FreeType integration for bitmap font atlas
- [ ] Session management UI (login screen, credential persistence on SD)
- [ ] Timeline view (reverse-chronological feed with cursor-based pagination)
- [ ] Post composition (text input via USB keyboard, submit via `createRecord`)
- [ ] Social actions (like, repost, follow)
- [ ] Avatar thumbnail rendering
- [ ] Error handling and retry logic for flaky WiFi
