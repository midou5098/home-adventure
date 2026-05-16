# Teskito Background Task

This SDL task demonstrates a playable background function for the game.

It includes:

- horizontal scrolling background
- vertical scrolling while jumping and falling
- fixed, moving, and destructible platforms
- idle, run, and jump player animations
- mono view and split-screen view with `M`

## Files

- `main.c`: starts SDL, runs the main loop, computes the camera, and chooses
  mono or split-screen rendering.
- `headers.h`: contains the structs, constants, tweak variables, and function
  prototypes documented by Doxygen.
- `fonctions.c`: implements image loading, background drawing, platform logic,
  player physics, collision, and animation rendering.

## How The Task Works

Every frame follows the same order:

1. SDL events are read, including quit, escape, and `M` for split-screen mode.
2. The keyboard state updates the player velocity and jump request.
3. Moving platforms update their X position.
4. Player physics applies horizontal movement, gravity, ground collision, and
   platform collision.
5. The horizontal camera follows the player, while the vertical camera follows
   the player during jumps and falls.
6. The background is drawn first, then platforms, then the player.

The background has four layers:

- far layer: slowest horizontal and vertical parallax
- middle layer: medium parallax
- near layer: stronger parallax
- ground layer: follows the camera exactly so the visual floor stays aligned
  with `GROUND_Y`

The player uses three sprite sheets:

- idle: used when standing still on the ground
- run: used when moving left or right on the ground
- jump: used while airborne

The platforms demonstrate the blueprint requirement:

- fixed platforms stay still
- moving platforms oscillate horizontally
- destructible platforms disappear after the player lands on them

## Controls

- `A` / left arrow: move left
- `D` / right arrow: move right
- `Space` / `W` / up arrow: jump
- `M`: switch mono/split-screen mode
- `Escape`: quit

## Background Tweaks

Edit these values in `headers.h`:

```c
#define BG_FAR_Y -50
#define BG_FAR_H_EXTRA 120
#define BG_MID_Y 0
#define BG_MID_H_EXTRA 120
#define BG_NEAR_Y -358
#define BG_NEAR_H_EXTRA 700
#define BG_GROUND_Y 548
#define BG_GROUND_H 150
```

Bigger Y values move a layer down. Smaller or negative Y values move a layer up.

The player floor collision still uses:

```c
#define GROUND_Y 548.0f
```

If you move `BG_GROUND_Y`, adjust `GROUND_Y` too when you want the playable
floor to match the visual ground.

## Generate Doxygen

From this directory:

```bash
make docs
```

The generated HTML starts at:

```text
docs/html/index.html
```
