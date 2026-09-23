# Cataclysm-DDA-glm53flash-eval — Architecture

## Overview

Cataclysm: Dark Days Ahead is a native, turn-based survival game. Most runtime behavior is C++17
under `src/`; game definitions, balance, maps, recipes, and mod content are JSON under `data/`.
The executable loads that data into registries, creates or loads a persistent world, and advances
the simulation in response to player and NPC turns.

This checkout is an evaluation worktree for crafting-menu performance. Its branch adds a
requirement index, delayed craftability checks, isolated profiling hooks, focused Catch2 tests,
and `tools/run_crafting_harness.ps1`. Keep evaluation-only instrumentation separable from normal
game behavior.

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Game and tools | C++17 |
| Content and configuration | JSON |
| Build | GNU Make or CMake; MSVC/vcpkg is supported on Windows |
| Tests | Catch2 test executable (`cata_test`) |
| UI | curses or SDL3 tiles build |
| Automation | Python, shell, and PowerShell scripts under `build-scripts/` and `tools/` |

## Data Flow

1. Startup loads core and selected mod JSON into typed registries such as recipes, items, mapgen,
   monsters, and professions.
2. A new or saved world supplies mutable map, character, vehicle, inventory, mission, and event
   state.
3. The game loop accepts an action, spends moves, updates actors and the world, and redraws the UI.
4. Crafting combines the recipe dictionary with the character's skills, known recipes, nearby
   inventory, tools, and component requirements.
5. On this branch, the crafting requirement index narrows candidate recipes before expensive
   apparent-craftability checks. Tests and the PowerShell harness measure the same pipeline with
   deterministic scenarios.

## Key Components

- `src/`: production game, UI, simulation, loaders, and command-line tools.
- `data/`: core JSON and bundled mods; content is validated independently of C++ compilation.
- `tests/`: Catch2 tests linked into `cata_test` or `cata_test-tiles`.
- `src/recipe_dictionary.*`: recipe registration and lookup.
- `src/crafting_gui*`: crafting-menu selection, filtering, and display pipeline.
- `src/crafting_requirement_index.*`: branch-specific index for requirement-driven candidate
  filtering.
- `tests/crafting_*`: correctness, scenario, and performance coverage for the evaluated path.
- `tools/run_crafting_harness.ps1`: repeatable local runner for crafting performance scenarios.

## Configuration / Environment Variables

CDDA primarily uses build variables and command-line options rather than a required application
environment file.

| Setting | Purpose | Default |
|---------|---------|---------|
| `TILES` | Build the graphical SDL variant with Make | `0` |
| `TESTS` | Build tests | enabled by the normal project build configuration |
| `RELEASE` | Select release-oriented compiler settings | `0` in developer Make builds |
| `LOCALIZE` | Include localization support | platform/build dependent |
| `VCPKG_ROOT` | Locate vcpkg for supported Windows CMake presets | unset |
| `--user-dir` | Isolate saves/config/cache for a game or test run | platform default user directory |

## External Dependencies

The core game has no required network service or database. Build dependencies vary by target and
include a C++ toolchain, gettext/localization tooling, curses for terminal builds, and SDL3 plus
its image/TTF dependencies for tiles builds. Windows CMake builds may resolve dependencies through
vcpkg. See `doc/c++/COMPILING.md` and the platform-specific compile guides for exact versions.
