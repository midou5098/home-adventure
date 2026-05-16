# Project Improvement Instructions

Use this file as the project improvement backlog. The items are ordered by impact and risk, based on a scan of the current C/SDL game code, level modules, online client, and Go relay server.

## Current Verification Snapshot

- `make clean && make` succeeds with no warnings in the current pass.
- `go test ./...` in `server/` succeeds.

## Completed In This Pass

- Level 2 online duo mode now uses a longer cardboard box spawn interval so streamed play has more reaction time.
- The previous level 3 build warnings were fixed.
- Online clients now resend held input masks at a short interval, improving remote input robustness during streaming.
- Online streaming now defaults to raw 1280x720 LAN frame packets at about 25 FPS, with receive fallback for older compressed frame packets.
- Duo and online skin selection now enforces unique skins; if one player takes the other player's skin, the other player is moved to the alternate skin.

## Highest Priority Fixes

### 1. Stabilize online streaming mode

Relevant files:

- `src/online/online_client.c`
- `src/online/online_client.h`
- `src/online/online_scene.c`
- `server/main.go`
- Calls to `online_client_submit_frame()` inside `lvls/*/main.c` and `lvls/newshoplvl1/game.c`

Current behavior:

- Host captures frames with `SDL_RenderReadPixels()`.
- Frames are downscaled to `960x540`.
- Frames are compressed with zlib level `1`.
- Frames are sent every `50 ms`, about 20 FPS.
- The Go server relays frame packets over TCP.
- The client sends input masks back to the host.

Required improvements:

1. Add streaming telemetry.
   - Track captured FPS, sent FPS, received FPS, dropped frame count, compressed frame size, outgoing queue size, and estimated network delay.
   - Expose this in a debug overlay in `online_render_remote_play()` and on the host while online gameplay is active.
   - Log summary stats when an online session ends.

2. Make frame quality adaptive.
   - Replace the fixed `ONLINE_CAPTURE_W`, `ONLINE_CAPTURE_H`, and `ONLINE_FRAME_INTERVAL_MS` values with runtime settings.
   - Start with `960x540 @ 20 FPS`.
   - If the send queue grows or frame delay rises, step down to `854x480`, then `640x360`, and/or lower FPS.
   - If the queue stays low for several seconds, step quality back up.
   - Keep only the latest video frame queued. Never let old video frames delay input, pause, save, start, end, lobby, or error packets.

3. Avoid blocking relay writes.
   - In `server/main.go`, do not write large frame packets directly from peer read goroutines.
   - Give each peer a bounded outbound channel and one writer goroutine.
   - For `packetFrame`, drop any older queued frame for that peer before queuing the new one.
   - Keep reliable control packets ordered and do not drop them unless the peer disconnects.

4. Separate video traffic from control traffic.
   - Keep input, pause, save, start, kick, end, lobby, and error packets small and prioritized.
   - If possible, use two TCP connections per room: one for control and one for video.
   - If keeping one TCP connection, enforce strict priority in both `online_client.c` and `server/main.go` so video backlog cannot delay input.

5. Reduce capture cost.
   - `SDL_RenderReadPixels()` can stall the renderer. Call `online_client_submit_frame()` only once per rendered frame and only after the final scene render.
   - Audit all call sites because some levels call `online_client_submit_frame()` in more than one path.
   - Consider rendering gameplay to a fixed-size streaming texture, then present that texture locally and encode from it. This avoids full-window readback and makes stream resolution independent of the local window size.

6. Improve compression and packet format.
   - Add a packet version byte to the frame payload before changing format.
   - Include timestamp, flags, format, width, height, sequence, raw size, compressed size, and optional quality level.
   - Try dirty-rectangle or delta-frame compression for mostly static scenes.
   - Benchmark zlib level `1` against faster alternatives available to the project. If adding a dependency is acceptable, evaluate LZ4 or zstd for lower CPU cost.

7. Improve input responsiveness.
   - Send input packets at a fixed small interval while a key is held, not only when the mask changes. This protects against lost disconnect/reconnect edges and server stalls.
   - Add sequence numbers to input packets.
   - On the host, ignore duplicate or old input sequence numbers.
   - Keep `TCP_NODELAY` enabled.

8. Add disconnect and slow-client handling.
   - Add read/write deadlines or idle timeouts in `server/main.go`.
   - Disconnect a client whose outbound queue stays over budget for several seconds.
   - Send a clear notice before disconnecting when possible, such as `CLIENT_TOO_SLOW`.

Acceptance checks:

- Online client can run 10 minutes without outgoing queue growth.
- Client input remains responsive while host is streaming.
- Pulling bandwidth down causes resolution/FPS to degrade instead of freezing.
- Restoring bandwidth allows quality to recover.
- Disconnecting either peer returns the other peer to a clean menu/lobby state.

### 2. Fix build warnings before new gameplay work

Relevant file:

- `lvls/level3-hell/main.c`

Instructions:

- Remove or use the unused local variable in `updateUtensils()`.
- Fix the unsigned comparison in `tickTouchAnim()` so the condition expresses the intended timing check.
- Replace `system("mkdir ...")` and `system("unzip ...")` in `tryExtractIntroZip()` with checked code paths or at least validate return values and fail gracefully.
- Remove unused static helpers if they are dead code, or wire them into the level if they are intended features.

Acceptance checks:

- `make clean && make` finishes with no warnings under the current `-Wall -Wextra` flags.

### 3. Improve level 2 online cardboard box spacing

Relevant file:

- `lvls/level2-chase/main.c`

Instructions:

- In online duo mode, increase the distance between level 2 cardboard boxes so streamed play gives the remote player more time to react.
- Keep solo/offline spacing unchanged unless testing proves the same spacing is also too tight there.
- Make the spacing value configurable with a named constant instead of a hard-coded number.
- Check that wider spacing does not break collision timing, jump routes, scoring, or enemy/chase pacing.
- Test with streaming enabled because input delay and video delay make close obstacles harder than they feel locally.

Acceptance checks:

- In online mode, consecutive cardboard boxes have clearly more space between them.
- The host and remote player see the same obstacle positions.
- Level 2 remains completable with both control schemes.
- Offline level 2 behavior is unchanged unless intentionally adjusted.

### 4. Split the largest gameplay files into modules

Largest files from the current scan:

- `lvls/level1-climb/main.c`: 6482 lines
- `lvls/level2-chase/main.c`: 4838 lines
- `lvls/level4-pool/main.c`: 3700 lines
- `lvls/newshoplvl1/game.c`: 3271 lines
- `src/online/online_client.c`: 1098 lines
- `src/online/online_scene.c`: 880 lines

Instructions:

- Split each large level into focused files: `assets.c`, `input.c`, `state.c`, `update.c`, `render.c`, `audio.c`, and `save.c`.
- Keep each level exposing one small `runLevelX()` entry point.
- Move online-specific calls into a small level integration helper where possible, so streaming behavior is consistent between levels.

Acceptance checks:

- Each extracted module has a narrow responsibility.
- The top-level `main.c` for each level is mostly orchestration.
- Build output is unchanged after extraction.

### 5. Remove process-wide `chdir()` dependencies

Relevant examples:

- `src/game/merged_levels.c`
- `lvls/launcher/main.c`
- `lvls/shared/session.c`
- `src/ui/ui_shared.c`
- `option/src/main.c`
- `lvls/menu/option in game/src/main.c`

Instructions:

- Resolve one absolute project root at startup.
- Pass explicit asset/save/config roots into levels and shared systems.
- Do not rely on changing the process working directory during gameplay.
- Keep saves and logs under one predictable data directory.

Acceptance checks:

- The game launches correctly from any current working directory.
- Tests do not need to `chdir()` into level folders.
- Packaging the game does not require fragile relative paths.

### 6. Consolidate duplicated options/menu code

Duplicated areas:

- `src/options/*`
- `option/src/*`
- `lvls/menu/option in game/src/*`

Instructions:

- Keep one shared settings persistence implementation.
- Keep one shared UI helper layer for common menu widgets.
- Let standalone option builds provide only their entry point and scene-specific assets.

Acceptance checks:

- A settings bug fix is made in one place.
- Standalone options and in-game options load the same saved values.

### 7. Finish level identity cleanup

Relevant issue:

- `lvls/level4-pool` still has legacy `level5` names in nearby shared progress/session code.

Instructions:

- Rename structures, fields, and functions to match the actual level identity.
- Add a save migration path so old save files still load.
- Avoid changing save keys without a migration.

Acceptance checks:

- New code uses consistent level numbering.
- Old saves continue to load.

### 8. Expand automated tests

Current gap:

- Build succeeds, but most fragile behavior is not covered by automated tests.

Instructions:

- Add tests for progress CSV read/write.
- Add tests for save migration.
- Add tests for session life carry between levels.
- Add tests for merged-level resume flow.
- Add server tests for room creation, join, host disconnect, client disconnect, frame relay, input relay, and slow-client queue dropping.

Acceptance checks:

- `make test` or an equivalent target runs all C tests.
- `go test ./...` covers relay behavior, not only compilation.

### 9. Move generated files out of the source tree

Instructions:

- Put binaries and object files under `build/`.
- Keep runtime saves under one `saves/` or user-data directory.
- Update `clean` targets to remove generated outputs from nested builds.
- Do not commit generated executables, object files, temporary extracted intro frames, or runtime saves.

Acceptance checks:

- A clean checkout contains source, assets, docs, and configs only.
- `make clean` returns the tree to a predictable state.

## Suggested Work Order

1. Fix current build warnings.
2. Stabilize online streaming telemetry and frame dropping.
3. Add adaptive stream quality.
4. Rework the Go relay writer path to prevent frame packets from blocking control packets.
5. Increase level 2 online cardboard box spacing and test it with streaming enabled.
6. Remove duplicate `online_client_submit_frame()` call paths.
7. Add online relay tests and C-side streaming smoke checks.
8. Remove `chdir()` dependencies.
9. Split the large gameplay files.
10. Consolidate duplicated options code.
11. Clean generated outputs and save paths.

## Do Not Change Without Care

- Save file keys and progress formats.
- Online packet numbers.
- Level entry point names used by merged gameplay.
- Asset paths used by packaged builds.
- Input mapping behavior for local duo mode.

When changing any of these, add a migration or compatibility layer first.
