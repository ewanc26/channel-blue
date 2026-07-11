# Channel Blue

A Bluesky client for the Nintendo Wii.

Browse your timeline, read posts, and compose replies — all from the comfort of
your couch, using the Wiimote and a USB keyboard. Built on the
[AT Protocol](https://atproto.com) via the
[wolfram](https://github.com/ewan-croft/wolfram) C SDK.

**Status:** MVP candidate. The Wii UI, USB-keyboard sign-in and composition
flows, bounded timeline controller, SD session persistence, Wolfram adapter,
and mbedTLS-backed Wii HTTPS transport are implemented and cross-compile to a
`.dol`. The UI boots in Dolphin; live PDS login and TLS still require final
verification on Wii hardware.

## Features

- Browse your home timeline (reverse-chronological feed)
- Read individual posts and threads
- Compose posts and replies (USB keyboard input)
- Like, repost, and follow
- Session persistence on SD card

## Requirements

### Hardware

- Nintendo Wii with Homebrew Channel installed
- SD card (FAT16 or FAT32)
- WiFi connection (802.11b/g)
- USB keyboard (recommended for composing posts)

### Software

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) toolchain
  (`devkitPPC`, `libogc`)
- Portlibs installed via `dkp-pacman`:
  - `freetype` (font rendering)
  - `libpng` (image decoding)
  - `zlib` (compression)
- [wolfram](https://github.com/ewan-croft/wolfram) C SDK (cross-compiled for
  PPC)

## Building

```sh
# Install the devkitPro toolchain (https://devkitpro.org/wiki/Getting_Started)
# Then install the Wii SDK and available portlibs:
sudo dkp-pacman -S wii-dev ppc-freetype ppc-libpng ppc-zlib

# Build libwolfram and its declared cJSON/libcbor dependencies for PPC:
../wolfram/tools/build_wii_mbedtls.sh
cmake -S ../wolfram -B ../wolfram/build-wii \
  -DCMAKE_TOOLCHAIN_FILE=../wolfram/.devdeps/wii.cmake \
  -DWOLFRAM_BUILD_WII=ON -DCMAKE_BUILD_TYPE=Release
cmake --build ../wolfram/build-wii -j

# Build Channel Blue
make

# Assemble dist/apps/channel-blue/ with a unique TLS entropy seed
make bundle
```

The output is `channel-blue.dol`.

`make bundle` generates a unique 64-byte TLS seed using OpenSSL and retains it
across rebuilds. Do not copy the same `entropy.bin` to more than one
installation. Channel Blue atomically rotates it before enabling TLS on every
launch. For a manually assembled installation, generate it with:

```sh
openssl rand 64 > entropy.bin
```

## Installing

Copy the build output to your SD card:

```
sd:/
  apps/
    channel-blue/
      boot.dol       ← channel-blue.dol (renamed)
      meta.xml       ← from repo root
      icon.png       ← from repo root
      entropy.bin    ← unique 64-byte TLS seed
```

Or deploy directly over WiFi:

```sh
wiiload channel-blue.dol
```

`wiiload` does not provide an SD-backed `entropy.bin`; install the bundle once
before using direct uploads. The file remains on SD and is rotated by Channel
Blue on each launch.

### Dolphin smoke test

Dolphin can boot the generated DOL for UI and controller testing:

```sh
make dolphin
```

The macOS launcher explicitly selects Dolphin's x86_64 slice because Dolphin
2606's ARM64 JIT can misdetect CPU features on some Apple Silicon Macs and
crash or refuse the JIT recompiler (forcing a fall back to the slow cached
interpreter). Running the x86_64 slice under Rosetta sidesteps the ARM64 JIT
entirely. NEON/AdvSIMD is mandatory in AArch64 and is not the cause. Other
platforms can launch `channel-blue.dol` directly.

For sign-in/network testing, enable Dolphin's virtual SD card and copy the
contents of `dist/` to its root so `sd:/apps/channel-blue/entropy.bin` exists.
Dolphin is useful for iteration, but final WiFi, clock, SD durability, and TLS
testing must be performed on Wii hardware.

Then launch **Channel Blue** from the Homebrew Channel.

## Controls

| Wiimote | Classic Controller | Action |
|---|---|---|
| D-pad Up/Down | Left stick | Scroll timeline |
| A | A | Select / Open post |
| B | B | Back / Cancel |
| Plus | R trigger | Load next page / refresh |
| Minus | L trigger | Compose post |
| 1 | X | Like selected post |
| 2 | Y | Repost selected post |
| Home | Start | Menu |

A USB keyboard is used for sign-in and post/reply text. Tab or Up/Down changes
the active sign-in field; Enter submits. Passwords are masked and never saved.

## Dependencies

| Library | Purpose | License |
|---|---|---|
| [wolfram](https://github.com/ewan-croft/wolfram) | AT Protocol / XRPC | MIT |
| [libogc](https://github.com/devkitPro/libogc) | Wii hardware abstraction | Various |
| [mbedTLS](https://github.com/Mbed-TLS/mbedtls) | TLS/HTTPS | Apache-2.0 |
| [FreeType](https://freetype.org/) | Font rendering | FreeType GPL/FTL |
| [libpng](http://www.libpng.org/) | PNG decoding | libpng license |
| [lwIP](https://savannah.nongnu.org/projects/lwip/) | TCP/IP stack | BSD |

## Roadmap

- [x] Makefile and Homebrew Channel DOL
- [x] Cross-compile and link libwolfram for PPC
- [x] WiFi initialisation through wolfram's Wii platform backend
- [x] Secure HTTPS transport with CA validation and rotating entropy seed
- [ ] Verify live HTTPS/login/timeline against bsky.social on Wii hardware
- [x] GX rendering pipeline (framebuffer, 2D quads, textures)
- [x] FreeType text rendering
- [x] Login screen and atomic session persistence
- [x] Timeline UI/controller with bounded cursor pagination
- [x] Post and reply composition via USB keyboard
- [x] Wolfram-backed like, repost, and follow operations
- [ ] Fetch and display avatar thumbnails
- [ ] Replace placeholder search, notifications, and profile tabs
- [ ] Error handling and WiFi retry logic

## Contributing

See [AGENTS.md](AGENTS.md) for code style, technical philosophy, and
development workflow.

## License

Channel Blue is licensed under the **GNU Affero General Public License v3.0 or
later** — see [LICENSE](LICENSE) for the full text.

The `wolfram` SDK it links against is MIT-licensed. The AGPL's network
interaction clause applies to Channel Blue's own source code.
