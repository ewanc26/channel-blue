# Channel Blue

A Bluesky client for the Nintendo Wii.

Browse your timeline, read posts, and compose replies — all from the comfort of
your couch, using the Wiimote and a USB keyboard. Built on the
[AT Protocol](https://atproto.com) via the
[wolfram](https://github.com/ewan-croft/wolfram) C SDK.

**Status:** MVP integration in progress. The Wii UI, USB-keyboard sign-in and
composition flows, bounded timeline controller, SD session persistence, image
pipeline, and concrete wolfram adapter are implemented and cross-compile to a
`.dol`. Live Bluesky access is still gated by wolfram's honest
`WF_ERR_NOT_IMPLEMENTED` Wii HTTPS/TLS transport.

## Features

- Browse your home timeline (reverse-chronological feed)
- Read individual posts and threads
- Compose posts and replies (USB keyboard input)
- Like, repost, and follow
- Avatar thumbnails
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
cmake -S ../wolfram -B ../wolfram/build-wii \
  -DCMAKE_TOOLCHAIN_FILE=../wolfram/.devdeps/wii.cmake \
  -DWOLFRAM_BUILD_WII=ON -DCMAKE_BUILD_TYPE=Release
cmake --build ../wolfram/build-wii -j

# Build Channel Blue
make
```

The output is `channel-blue.dol`.

## Installing

Copy the build output to your SD card:

```
sd:/
  apps/
    channel-blue/
      boot.dol       ← channel-blue.dol (renamed)
      meta.xml       ← from repo root
      icon.png       ← from repo root
```

Or deploy directly over WiFi:

```sh
wiiload channel-blue.dol
```

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
- [ ] Secure HTTPS connectivity to bsky.social (TLS/entropy backend)
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
