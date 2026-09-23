# Cataclysm-DDA-crafting-eval — Data Models

## Entities

- `Character`, `avatar`, and `npc`: actors with stats, skills, effects, inventory, activities, and
  position in a world.
- `item` / item type definitions: runtime objects and their immutable JSON-loaded definitions.
- `recipe` and `recipe_dictionary`: craft definitions and the registry used to discover them.
- `requirement_data`: component, tool, quality, and resource requirements shared by recipes and
  construction.
- `inventory`: a query-oriented view over items available from characters, maps, and vehicles.
- `map`, `mapbuffer`, `vehicle`, and overmap types: persistent world state at different scales.
- `game`: top-level runtime coordinator for the active world, UI, and turn processing.

Branch-specific model:

- `crafting_requirement_index`: derives lookup keys from recipe requirements and maps changed
  inventory capabilities to candidate recipes. It is an acceleration structure; final
  craftability remains authoritative in the existing crafting checks.
- Charged-tool requirement options retain both the full charge threshold and the batch-one
  `start_only` threshold. The result cache stores both outcomes; inexact facts return `unknown`
  so the crafting menu uses the legacy requirement check.
- Stored digital items and same-type nested containers mark their item facts inexact to preserve
  legacy binned-query behavior. Fault-broken items remain eligible for unit counts, while charge
  counts reject both faults and the explicit broken flag, matching the legacy queries.

## DTOs

This is not a service-oriented application, so it has no conventional HTTP DTO layer. Its main
boundaries are:

- JSON objects loaded from `data/` into typed C++ factories and registries.
- Save-game JSON written beneath the selected user directory and loaded back into runtime types.
- Command-line arguments and text output used by the game, validators, tests, and benchmark tools.
- Catch2 scenario inputs and measurements emitted by the crafting performance harness.

## Enums

Enums are distributed with their owning domains rather than held in one central module. Common
families include body parts, damage types, item phases, character effects, activities, seasons,
terrain/furniture flags, and crafting/filter states. Prefer the owning header and its string-id or
JSON conversion helpers as the source of truth; `src/all_enum_values.h` supplies complete-value
helpers for supported enums.

## Repository Reference

- Static definitions: `data/core`, `data/json`, `data/raw`, and `data/mods`.
- Production C++: `src/`, generally one `.h`/`.cpp` pair per domain concept.
- Tests and fixtures: `tests/` and test-only user directories selected with `--user-dir`.
- Local runtime state: `config/`, `save/`, `memorial/`, and cache directories; these are ignored.
- Build products: `obj/`, `objwin/`, `build/`, and test executables; these are ignored.

JSON factories and registries generally resolve string identifiers after loading. Runtime code
should use existing typed IDs and registries rather than scanning JSON or duplicating ownership.
Save compatibility matters: changes to serialized fields need defaults or migration behavior for
older worlds.
