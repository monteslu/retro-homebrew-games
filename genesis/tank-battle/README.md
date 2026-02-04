# Tank Battle

Classic top-down tank combat for Sega Genesis / Mega Drive.

## Gameplay

- **1 Player**: Fight against AI tanks
- **2 Player**: Local deathmatch
- **Win condition**: First to 5 kills

Each round generates a new random arena with walls and obstacles.

## Controls

| Button | Action |
|--------|--------|
| D-Pad  | Move tank |
| A/B/C  | Fire |
| Start  | Pause / Start game |

## Screenshots

*Coming soon*

## Building

From repo root:
```bash
./build.sh genesis tank-battle
```

Or directly:
```bash
docker run --rm -v "$PWD:/src" ghcr.io/stephane-d/sgdk:latest make -f /sgdk/makefile.gen
```

Output: `out/rom.bin`

## Technical

- **Engine**: SGDK (Sega Genesis Development Kit)
- **Language**: C
- **Resolution**: 320x224
- **Players**: 1-2

## License

MIT
