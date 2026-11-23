# Tower Jump: C++ edition

A C++17 rewrite of my Unity game [Tower Jump](https://maneghazaryan.github.io/tower-jump/), an
Icy Tower–style vertical climber. Run along a floor to build speed, turn it into a jump that
clears several floors at once, bounce off the walls, and keep climbing before the tower's
camera catches you.

**▶ Play in the browser: https://maneghazaryan.github.io/tower-jump-cpp/**

| | Keyboard | Touch |
|---|---|---|
| Run | `A` / `D` or `←` / `→` | left and middle pads |
| Jump | `Space`, `W` or `↑` | right pad |

## Why this exists

I've built games in Unity for 8+ years. I wanted to build the same game without an engine: write
the game loop, the entity system, the physics and the build pipeline myself, then compare it
with how Unity does it for me.

The game reads **the same JSON config files** as the Unity version (`assets/config/`), so both
builds share the same movement feel, tower layout and difficulty curve.

## Architecture

```
src/core/            the game, no graphics: builds and tests on any machine
  config.*           JSON config loading + validation (reports every error, by file and field)
  rng.hpp            PCG32: same seed gives the same tower on every platform and compiler
  tower.*            deterministic tower generator (bands for gaps, widths, platform types)
  ecs.hpp            minimal entity-component system (sparse sets, generational ids)
  game.*             simulation: movement, jumps, wall bounces, one-way platforms, combo, camera
  fixed_timestep.hpp fixed-step accumulator with render interpolation
src/game/            raylib front end: window, input, rendering
tests/               doctest unit tests (run under AddressSanitizer + UBSan in CI)
tools/sim.cpp        headless bot that plays the game, for tuning and regression checks
```

**Simulation separated from presentation.** `tj::Game` takes an `InputState` and a fixed `dt` and
emits events (`Land`, `WallBounce`, `ComboEnd`, …). It doesn't know about raylib, the clock or the
keyboard. That makes it deterministic (same inputs give the same run, which a test checks) and testable
without a window. The renderer turns events into effects.

**Fixed timestep.** Physics runs at 120 Hz no matter the display rate, with an accumulator and
interpolation between the last two states for smooth drawing. Jump heights are identical at 30,
60 or 144 FPS. Long frames, like a background tab, are clamped instead of spiralling.

**Entity-component system.** Each component type lives in a `SparseSet<T>`: a packed array of
components plus a sparse index from entity to slot. Iteration is a linear walk over contiguous
memory. Add, remove and lookup are O(1), and removal swaps the last element into the hole.
Entity ids carry a generation counter, so a destroyed platform's stale handle can't read
the platform that reused its slot.

**Pooling.** Platforms stream in above the camera and out below it. The registry recycles entity
slots through a free list, and component storage is reserved up front (`platformPoolPrewarm`).
A run up the 400-floor tower keeps ~20 entities alive and allocates nothing after warm-up. A test
checks this.

**One-way platforms by history.** A platform is solid only if the player's feet were above its
surface at the end of the *previous* step. So you jump up through platforms, run through their
sides, and can never get wedged between a platform and a wall.

**Grip and jump charge.** The landing speed is banked for the next jump *before* surface grip is
applied. Grip removes drift but never the speed the player is holding a key for. Ice keeps 92%
of the drift, stone keeps none. Standing still for 0.25 s spends the charge.

## Build

Requires CMake 3.20+ and a C++17 compiler. Dependencies (raylib 5.5, nlohmann/json, doctest) are
fetched automatically.

```bash
cmake -S . -B build
cmake --build build -j
./build/tower_jump                 # play
ctest --test-dir build             # unit tests
./build/tj_sim 60 5                # headless bot, 5 runs of 60 s
```

Linux needs the X11/OpenGL dev packages:
`sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`

Web build (Emscripten):

```bash
emcmake cmake -S . -B build-web -DPLATFORM=Web -DTJ_BUILD_TESTS=OFF
cmake --build build-web -j        # produces index.html / .js / .wasm / .data
```

CI (GitHub Actions) builds natively with AddressSanitizer + UndefinedBehaviorSanitizer, runs the
tests and the headless simulation, builds the WebAssembly version, and publishes it to GitHub
Pages.

## Compared with the Unity version

| | Unity | This port |
|---|---|---|
| Game loop | `Update` / `FixedUpdate` | own accumulator, 120 Hz, interpolated rendering |
| Objects | GameObjects + MonoBehaviours | sparse-set ECS, plain structs |
| Physics | Rigidbody2D + custom filter | own integration and swept one-way landing test |
| Pooling | `ObjectPool<T>` | generational id free list + reserved component storage |
| Config | ScriptableObjects + JSON | the same JSON files, own validation |
| Builds | Unity WebGL / Android / iOS | CMake: native + Emscripten, CI on GitHub Actions |

## Roadmap

- [ ] Sprites and sound (the Unity build's art)
- [ ] Persist the best score (local file / `localStorage`)
- [ ] Collectibles and the rocket item from `items.json`
- [ ] More towers from the Unity config set
