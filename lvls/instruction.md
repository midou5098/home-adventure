# Verified Code Scan (Updated 2026-04-05)

## Applied Fixes

1. Top wall covers are no longer hard-capped to 2.
   - Updated loop in `newshoplvl1/map.c` to process up to `MAX_TOP_WALL_COVERS`.
   - Updated cover/obstacle vertical offset logic to be row-based instead of index-based.
   - Synced draw logic in `newshoplvl1/game.c` to match map obstacle placement.

2. Rectangle hit-test boundary fixed in billiards UI.
   - `newshoplvl1/billiards.c:point_in_rect` now uses exclusive right/bottom bounds (`<`) to match SDL rect semantics.

3. Defensive keyboard scancode bounds checks added.
   - `newshoplvl1/input.c` now validates scancodes before indexing `keys[]` / `just_pressed[]`.
   - `input_down` and `input_pressed` now return `false` for invalid scancodes.

4. Dialogue wrapping string handling hardened.
   - `level4-pool/main.c:wrapText` now uses bounded `SDL_strlcpy` / `SDL_strlcat` for intermediate buffers.

5. Door label buffer warning removed.
   - `level4-pool/main.c` door label buffer increased from `char buf[8]` to `char buf[16]`.

6. Unused function cleanup.
   - Removed unused `drawShopIcon` helper in `level1-climb/main.c`.

## Verification Notes (Previous Report Corrections)

- `newshoplvl1/game.c` sprite-sheet division is already guarded by `cols > 0 && rows > 0`.
- `newshoplvl1/game.c` ad-frame modulo is already guarded by `if (game->ad_frame_count > 0)`.
- `launcher/main.c` font-failure path does not dereference NULL directly; text creation returns NULL and callers early-return.
- `newshoplvl1/admin.c` and `shared/session.c` already check `fread` / `fwrite` return values in the current implementation.

## Build Verification

Strict build command used:

```bash
make clean && make CFLAGS='-O0 -g -std=c11 -DMERGED_BUILD -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wnull-dereference'
```

Result: build succeeded with no warnings.
