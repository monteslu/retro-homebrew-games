# Free Retro Games

A collection of free, open-source homebrew games for retro consoles.

[![Build ROMs](https://github.com/monteslu/free-retro-games/actions/workflows/build.yml/badge.svg)](https://github.com/monteslu/free-retro-games/actions/workflows/build.yml)

## Downloads

**[Latest Release](https://github.com/monteslu/free-retro-games/releases/latest)** — Pre-built ROMs ready to play.

Or build from source (see below).

## Games

### Genesis / Mega Drive

| Game | Description | Players |
|------|-------------|---------|
| [Tank Battle](genesis/tank-battle/) | Top-down tank combat arena | 1-2 |
| [Battle 4Tris](genesis/battle-4tris/) | Competitive 4Tris with garbage lines | 1-2 |
| [Pong](genesis/pong/) | Classic paddle game | 1-2 |
| [Snake Arena](genesis/snake/) | Classic snake with competitive mode | 1-2 |
| [Space Shooter](genesis/space-shooter/) | Vertical shooter with co-op | 1-2 |
| [Breakout](genesis/breakout/) | Brick-breaking action | 1-2 |

### Coming Soon

- NES games
- Game Boy / Game Boy Color games
- Master System games

## Playing

Load the `.bin` files in any emulator:

- **Genesis**: BlastEm, Gens, Fusion, RetroArch (Genesis Plus GX)
- **Flash carts**: Everdrive, MegaSD, etc.
- **[retroterm](https://github.com/monteslu/retroterm)**: Terminal-based retro gaming

## Building Locally

### Prerequisites

- Docker (for containerized builds)

### Build All Games

```bash
./build.sh
```

### Build Specific System

```bash
./build.sh genesis
```

### Build Specific Game

```bash
cd genesis/tank-battle
docker run --rm -v "$PWD:/src" ghcr.io/stephane-d/sgdk:latest make -f /sgdk/makefile.gen
# Output: out/rom.bin
```

## Project Structure

```
free-retro-games/
├── genesis/           # Sega Genesis / Mega Drive games
│   └── tank-battle/
│       ├── src/       # C source code
│       ├── res/       # Resources (graphics, sound)
│       └── out/       # Build output (rom.bin)
├── nes/               # NES games (coming soon)
├── sms/               # Master System games (coming soon)
├── .github/workflows/ # CI/CD
└── build.sh           # Local build script
```

## Contributing

Want to add a game? 

1. Create a folder under the appropriate system (`genesis/`, `nes/`, etc.)
2. Add source code following the system's SDK conventions
3. Test locally with `./build.sh`
4. Submit a PR

All games must be:
- Original work or properly licensed
- Free to distribute
- Family-friendly

## Development

### Genesis (SGDK)

Games are built with [SGDK](https://github.com/Stephane-D/SGDK). Resources:

- [SGDK Wiki](https://github.com/Stephane-D/SGDK/wiki)
- [SGDK Samples](https://github.com/Stephane-D/SGDK/tree/master/sample)
- [Ohsat Games Tutorials](https://www.ohsat.com/tutorial/)

### NES (cc65)

Coming soon. Will use [cc65](https://cc65.github.io/) toolchain.

## License

All games are released under the MIT License unless otherwise noted in their directory.

## Credits

Built for [retroterm](https://github.com/monteslu/retroterm) — retro gaming in your terminal.
