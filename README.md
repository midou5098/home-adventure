# Home Adventure

![C](https://img.shields.io/badge/C-SDL2-2d6cdf?style=for-the-badge&logo=c&logoColor=white)
![Go](https://img.shields.io/badge/Go-online%20relay-00add8?style=for-the-badge&logo=go&logoColor=white)
![Python](https://img.shields.io/badge/Python-camera%20minigames-ffd43b?style=for-the-badge&logo=python&logoColor=222)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20SDL2-222?style=for-the-badge)

**Home Adventure** is a custom C/SDL2 adventure game built around a cinematic main menu, character selection, solo/duo play, an online lobby, persistent saves, score tracking, camera-powered bonus content, and a sequence of handmade levels.

The game moves from a stealthy vertical climb through chase scenes, trap rooms, a shop hub, arcade-style interactions, a pool/volleyball challenge, and a final cutscene. It is structured as one merged SDL runtime, with level modules, shared UI helpers, save/progress systems, and a small Go relay server for online play.

## Preview

```text
main menu
  -> load scene
  -> solo / duo / online mode
  -> character, name, skin, and control selection
  -> Level 1: The Sneak
  -> Level 2: chase and mini transition sections
  -> Level 3: trap gauntlet and maze-style challenges
  -> Level 4: pool / volleyball-style challenge
  -> final cutscene
  -> top scores and leaderboard
```

## Features

- **Merged SDL2 game shell** with animated menus, fade transitions, shared renderer/window ownership, music, effects, and reusable UI helpers.
- **Solo and local duo modes** with separate player skins, control choices, lives, level results, and score carry-over.
- **Online mode** with host/join lobby, room codes, streamed remote frames, input relay, pause/save notifications, and level synchronization.
- **Character selection** with animated skin previews, name entry, snow effects, controller-aware control choices, and unique duo skin enforcement.
- **Progression and saves** across solo and duo files, including checkpoints, resumable runs, best score storage, and leaderboard entries.
- **Level 1 climb** with height scoring, keys, lives, shop transitions, powerups, final dialogue, and chapter presentation.
- **Level 2 chase** with traps, hearts, dialogue, key events, mini-level transitions, duo handling, and level summary grading.
- **Level 3 trap gauntlet** with layered hazards, solo/duo paths, online synchronization, maze-style sections, and timed challenge handling.
- **Level 4 pool challenge** with aim/power controls, active-player logic, and level completion scoring.
- **Shop level** with player movement, interaction targets, audio, map systems, barista/admin logic, billiards quiz, and arcade input wrapper.
- **Options menu** with audio sliders, brightness/fullscreen settings, credits, pause overlay behavior, and map access.
- **Camera extras** including a hand-camera minigame environment and a Linux SDL2/V4L2 face-capture app that creates transparent face cutouts.
- **Developer documentation** through Doxygen pages in `docs/`.

## Controls

Controls are selected in the choose scene and carried into the level runner.

| Input scheme | Movement | Jump / action | Interact |
| --- | --- | --- | --- |
| WASD | `W` `A` `S` `D` | `W` or `Space` where supported | usually `F` |
| Arrows | Arrow keys | `Up` where supported | usually `0` / keypad `0` |
| Controller | Arcade/controller mapping | mapped button | mapped interact button |

Useful runtime keys:

- `Esc`: pause, back, or quit current overlay depending on the scene.
- `M`: open the solo game map while inside a solo level or final cutscene.
- Map view: `F` toggles zoom/full map, mouse drag pans while zoomed, `M` or `Esc` returns.
- `Shift + A`: skip one level in test builds.
- `Shift + K`: jump to the shop from the main menu.
- Camera app: `Space` or `C` captures a photo, `Esc` exits.

## Tech Stack

| Area | Technology |
| Game runtime | C, SDL2, SDL2_image, SDL2_ttf, SDL2_mixer |
| Online relay | Go TCP/UDP server |
| Face capture | C, SDL2, V4L2, libjpeg, Python |
| Image processing | OpenCV, rembg, Pillow, ONNX Runtime |
| Build | Make, CMake |
| Docs | Doxygen |

## Project Layout

```text
.
|-- src/
|   |-- main_menu/       # main loop, scene routing, menu rendering
|   |-- load/            # solo / duo / online and save/start selection
|   |-- choose/          # player name, skins, controls
|   |-- options/         # options scene and settings persistence
|   |-- online/          # lobby UI and game client
|   |-- top_scores/      # best score and leaderboard display
|   |-- enigma/          # bonus/enigma scene
|   |-- game/            # progress, session, map, final cutscene, level runner
|   `-- ui/              # shared SDL asset/font/button helpers
|-- lvls/
|   |-- level1-climb/    # vertical climb and shop handoff
|   |-- level2-chase/    # chase level and mini sections
|   |-- level3-hell/     # trap gauntlet and maze sections
|   |-- level4-pool/     # pool / volleyball-style level
|   |-- newshoplvl1/     # shop, billiards, arcade, map, barista/admin systems
|   `-- shared/          # shared level session and arcade input utilities
|-- assets/              # menus, sounds, cutscenes, camera helpers, fonts
|-- server/              # Go online relay server
|-- saves/               # runtime save, score, and leaderboard files
|-- docs/                # Doxygen Markdown pages
`-- Makefile             # merged game build
```

## Install Dependencies

Ubuntu/Debian:

```sh
sudo apt update
sudo apt install -y build-essential gcc make pkg-config cmake unzip ffmpeg \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
  libjpeg-dev python3.11 python3.11-venv python3-pip v4l-utils golang-go
```

Arch/Manjaro:

```sh
sudo pacman -S --needed base-devel gcc make pkgconf cmake unzip ffmpeg \
  sdl2 sdl2_image sdl2_ttf sdl2_mixer libjpeg-turbo python python-pip go v4l-utils
```

## Python Environments

The game auto-detects `venv311` when launching the hand-camera minigame. If it is missing or broken, recreate it from the project root:

```sh
python3.11 -m venv venv311
venv311/bin/python -m pip install --upgrade pip
venv311/bin/python -m pip install -r assets/hand_shape_enigme/requirements.txt
```

Face capture uses a separate environment:

```sh
python3.11 -m venv assets/face_capture/.venv
assets/face_capture/.venv/bin/python -m pip install --upgrade pip
assets/face_capture/.venv/bin/python -m pip install opencv-python rembg pillow onnxruntime
```

## Build

Run all commands from the repository root:

```sh
make clean
make -B
make face-capture
```

The main executable is created as:

```text
./game
```

## Run

```sh
./game
```

Run the executable from the project root. Most assets are resolved relative to the root, but launching from here avoids path issues in older level and minigame code.

## Online Mode

Start the relay server on the host machine:

```sh
cd server
go run .
```

Or use the bundled server binary if available:

```sh
cd server
./homeadventure-online-server
```

The default port is `9090`. To use another port:

```sh
PORT=9091 go run .
```

Then start `./game` on both machines, choose **Online Mode**, and host or join using the room code. For local testing, keep both machines on the same LAN and make sure the selected port is reachable over TCP and UDP.


## Camera Notes

If the hand-camera minigame or face capture cannot open a camera:

```sh
v4l2-ctl --list-devices
sudo usermod -aG video "$USER"
```

After adding the user to the `video` group, log out and log back in.

The face-capture tool is Linux-specific in its current form because its capture backend uses V4L2. The SDL UI and Python processing pipeline are portable, but Windows/macOS would need a different camera backend.

## Documentation

Generate Doxygen docs:

```sh
make docs
```

Open the generated HTML at:

```text
docs/doxygen/html/index.html
```

Clean generated docs:

```sh
make docs-clean
```

## Testing Helpers

The repository includes focused C tests for progress and settings behavior:

```sh
./tests/game_progress_test
./tests/options_settings_test
```

If the binaries are missing or stale, rebuild the project first.

## Bug Reports

Useful details to include:

- Mode: solo, duo, or online.
- Level and exact action before the issue.
- Terminal output from `./game`.
- Whether the issue happens again after restarting.
- For online issues: host/client role, server IP/port, and whether both machines used the same build.

## Credits

Home Adventure is a custom student-style SDL game project with original gameplay modules, menu systems, level integrations, camera tools, and online relay work organized into one merged build.
