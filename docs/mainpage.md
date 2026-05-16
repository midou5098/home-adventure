# Home Adventure SDL Game

This documentation is generated with Doxygen from the C source code and the Markdown pages in `docs/`.

## Generate The Documentation

Install Doxygen, then run:

```bash
make docs
```

The generated HTML starts at:

```text
docs/doxygen/html/index.html
```

To remove generated documentation:

```bash
make docs-clean
```

## Project Overview

The project is an SDL-based game with a main menu, character selection, options, top scores, an enigma scene, multiple gameplay levels, a shop level with arcade minigames, save/progress handling, and online lobby/client support.

Main source areas:

- `src/main_menu/`: main menu entry point, menu widgets, animated background, and scene helpers.
- `src/load/`: game mode and save/start selection scene.
- `src/choose/`: player name, character skin, and control selection scene.
- `src/options/`: in-game options scene and settings persistence.
- `src/top_scores/`: best score display scene.
- `src/enigma/`: random enigma/bonus scene.
- `src/game/`: shared progress, skin registry, merged level runner, and reusable level runtime helpers.
- `src/online/`: online lobby scene and TCP client used by the game.
- `src/ui/`: shared SDL asset loading, font, and button rendering helpers.
- `lvls/shared/`: level session state and shared arcade input utilities.
- `lvls/level1-climb/`, `lvls/level2-chase/`, `lvls/level3-hell/`, `lvls/level4-pool/`: playable level implementations.
- `lvls/newshoplvl1/`: shop level, map/player systems, billiards quiz, audio, and arcade wrapper.
- `option/src/`: standalone extracted options menu.

The Go server in `server/` is part of the repository, but Doxygen is focused on the C/SDL code. Document server behavior separately if it grows beyond the current simple lobby relay.

## Useful Entry Points

- `main()` in `src/main_menu/main.c` starts the merged game build.
- `merged_levels_run()` runs the playable level sequence.
- `GameSelection` carries the selected solo/duo mode, skins, controls, resume flag, and save behavior from menus into gameplay.
- `GameProgress` stores persistent save data, unlocked levels, score, options, and level completion state.
- `GameSession` carries per-run level results and score calculations across level modules.
- `game_run()` starts the standalone shop level runtime.

## Architecture Page

See @ref architecture for a deeper map of how menus, progression, levels, and online play fit together.
