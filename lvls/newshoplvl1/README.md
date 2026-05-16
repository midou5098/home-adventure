# SDL Version Shop

This folder contains a C/SDL2 port of the `shop/` JavaScript game.

## Build

```bash
cd sdlversionshop
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
cd sdlversionshop/build
./sdlversionshop
```

## Controls

- `W/A/S/D` or arrow keys: move
- `F`: interact (billiards table / barista / arcade machine / top-left door exit)
- `A` / `B`: answer billiards quiz
- While barista shop window is open:
  - `Up` / `Down`: select item
  - `Enter`: buy selected item
  - `Escape`: close shop window
- `F1`: toggle layout admin editor (TV + animated ad + NPC)
- While layout admin editor is open:
  - `1`/`T`: edit TV transform
  - `2`/`G`: edit animated ad transform
  - `3`/`V`: edit arcade transform
  - `4`/`C`: edit arcade popup transform
  - `5`/`B`: edit NPC entries
  - `N`: add NPC (up to 16)
  - `Delete`/`Backspace`: remove selected NPC
  - `[` / `]`: choose NPC entry
  - `Q` / `E`: change NPC skin
  - `F`: flip selected NPC on X axis
  - `Z`: select NPC Z-index field, then use arrows to change it
  - `X` / `Y` / `W` / `H` or `Tab`: choose field to edit
  - `Arrow keys` (or `A` / `D`): adjust selected value
  - `Shift`: faster adjustment
  - `Enter`: save transforms
  - `R`: reset selected target transform
  - `Escape`: close layout admin editor
- Players cannot walk through NPCs.
- Left click: quiz options or close button
- `Escape`: close quiz popup (or quit if popup is closed)
