@page architecture Architecture

# Architecture

## Runtime Flow

The merged build starts in `src/main_menu/main.c`. It owns the SDL window/renderer and moves through scene states for the main menu, load scene, character selection, options, top scores, enigma, online lobby, and gameplay.

The normal local game flow is:

```text
main menu -> load scene -> choose scene -> merged level runner -> playable levels
```

Online hosting uses the lobby scene first, then starts gameplay with an online-enabled `GameSelection`.

## Scene Pattern

Most scenes follow the same small API shape:

```text
*_init()
*_enter()
*_handle_event()
*_update()
*_render()
*_leave()
*_cleanup()
```

Scene result structs such as `LoadSceneResult`, `ChooseSceneResult`, `OptionsSceneResult`, and `OnlineSceneResult` expose navigation requests back to the owner loop.

## Shared State

`GameSelection` is short-lived startup configuration. It records solo/duo mode, selected skins, control schemes, whether a save should be resumed, and whether checkpoints should be kept.

`GameProgress` is persistent save data. It tracks player identity, selected options, current level, unlocked levels, score, lives, and completion data for each level.

`GameSession` is per-run level state. It carries individual level results, score totals, grade calculation, selected controls, and life carry-over between levels.

## Level Modules

The primary playable level folders are:

- `lvls/level1-climb/`: climb/platform level and shop/powerup interactions.
- `lvls/level2-chase/`: chase level, traps, key selection, and mini transition sections.
- `lvls/level3-hell/`: trap gauntlet level with solo, duo, and online-hosted duo runtime paths.
- `lvls/level4-pool/`: pool/volleyball style level currently using several legacy `level5` save/result names.
- `lvls/newshoplvl1/`: shop area with interaction targets, billiards quiz, audio, player movement, and arcade wrapper.

`src/game/merged_levels.c` coordinates the level sequence for the merged build.

## Assets And Paths

Asset path constants live mainly in:

- `src/main_menu/asset_paths.h`
- `option/src/asset_paths.h`

Shared helpers in `src/ui/ui_shared.c` resolve project-relative asset paths, load textures/music/sounds, open fonts, and render common UI elements.

## Persistence

Save and score files live under `saves/` at runtime:

- `saves/last_game.csv`
- `saves/last_game_solo.csv`
- `saves/last_game_duo.csv`
- `saves/best_score.csv`

Settings are stored in small binary files such as `settings.dat` and `global_settings.dat`.

## Online Play

`src/online/online_client.c` handles the game client's connection, lobby state, input masks, pause state, save notifications, and streamed frame snapshots.

`server/main.go` implements the lightweight relay/lobby server. It is not parsed by this Doxygen setup because Doxygen is configured for the C/SDL source tree.
