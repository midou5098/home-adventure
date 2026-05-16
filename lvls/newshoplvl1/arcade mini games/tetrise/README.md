# Tetris in C with SDL2

Minimal playable Tetris built as a single C file with SDL2.
Background music is loaded from `tetrise bg.mp3` via `SDL2_mixer`.

## Build

```sh
make
```

This now requires `SDL2_mixer` in addition to `SDL2`.

## Run

```sh
./tetris
```

## Controls

- `Left` / `A`: move left
- `Right` / `D`: move right
- `Down` / `S`: soft drop
- `Up` / `X`: rotate clockwise
- `Z`: rotate counter-clockwise
- `Space`: hard drop
- `P`: pause
- `R`: restart

## Notes

- Score, lines, level, and next piece are shown in the window title.
- The right panel is intentionally asset-free, so the project stays self-contained.
