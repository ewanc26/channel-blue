# Channel Blue

A Bluesky client for the Nintendo Wii.

Browse your timeline, read posts, and compose replies — all from the comfort of
your couch, using the Wiimote and a USB keyboard. Built on the
[AT Protocol](https://atproto.com) via the
[wolfram](https://github.com/ewan-croft/wolfram) C SDK.

**Status:** Early development. The repo contains project scaffolding and
metadata; no source code has been committed yet.

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
  - `mbedtls` (TLS for HTTPS)
  - `freetype` (font rendering)
  - `libpng` (image decoding)
  - `zlib` (compression)
- [wolfram](https://github.com/ewan-croft/wolfram) C SDK (cross-compiled for
  PPC)

## Building

```sh
# Install the devkitPro toolchain (https://devkitpro.org/wiki/Getting_Started)
# Then install portlibs:
sudo dkp-pacman -S wii-dev mbedtls freetype libpng zlib

# Build libwolfram for PPC (see wolfram repo for instructions)

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
| Plus | R trigger | Next page |
| Minus | L trigger | Previous page |
| Home | Start | Menu |

A USB keyboard can be used to type post text.

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

- [ ] Makefile and hello-world DOL
- [ ] Cross-compile mbedTLS and libwolfram for PPC
- [ ] WiFi init + HTTPS connectivity to bsky.social
- [ ] GX rendering pipeline (framebuffer, 2D quads, textures)
- [ ] FreeType bitmap font atlas
- [ ] Login screen and session persistence
- [ ] Timeline view with cursor pagination
- [ ] Post composition via USB keyboard
- [ ] Social actions (like, repost, follow)
- [ ] Avatar thumbnails
- [ ] Error handling and WiFi retry logic

## Contributing

See [AGENTS.md](AGENTS.md) for code style, technical philosophy, and
development workflow.

## License

Channel Blue is licensed under the **GNU Affero General Public License v3.0 or
later** — see [LICENSE](LICENSE) for the full text.

The `wolfram` SDK it links against is MIT-licensed. The AGPL's network
interaction clause applies to Channel Blue's own source code.

## Support
If you find this project useful, consider supporting its development:
[![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/ewancroft)
[![GitHub Sponsors](https://img.shields.io/badge/GitHub%20Sponsors-30363D?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/ewanc26)
