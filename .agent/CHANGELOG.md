# Changelog — Cataclysm-DDA-crafting-eval

## 2026-09-23 — Preserve legacy inventory query behavior

- `src/crafting_requirement_index.*`: fall back for stored digital items and same-type nested
  containers; distinguish broken-item flags from faults for unit and charge counts.
- `tests/crafting_requirement_index_test.cpp`: cover stored software/books, damaged devices,
  nested containers, and legacy full-charge and craft-start evaluation.
- `.agent/data_models.md`: document the additional conservative snapshot cases.

## 2026-09-23 — Cache exact craft-start tool charges

- Added craft-start charge thresholds to indexed recipe options and cached results, preserving
  the existing full-charge evaluation for other callers.
- Routed batch-one crafting-menu availability through the craft-start result, with legacy fallback
  for inexact charge sources including UPS aliases and connected power.
- Extended requirement-index equivalence and boundary tests and updated Armor harness bypass
  reporting to count only actual tool-charge fallbacks.

## 2026-09-23 — Reuse nested recipe availability

- Routed normal crafting-list entries and nested expansion through one shared availability resolver.
- Added cycle protection and retained the existing constructor ABI for unchanged callers.
- Added focused coverage proving an unexpanded nested child is cached and reused by later list builds.

## 2026-09-23 — Emulate the fresh-process Armor workflow

- Changed the crafting harness to run the Armor pipeline in three independent test processes.
- Added fail-closed semantic-count comparison across runs and a schema-v2 aggregate containing
  per-run evidence plus median cold, warm, sorted, and expanded timings.
- Kept the harness at the crafting logic boundary; native SDL input and rendering remain outside
  its claimed coverage.

## 2026-09-23 — Remove the model name from the project

- Renamed the linked worktree and project identity to `Cataclysm-DDA-crafting-eval`.
- Kept the existing evaluation branch and remote branch unchanged.

## 2026-09-22 — Finish local scaffold without Menhir

- Added repository-boundary, branch, local artifact, and deferred-Menhir guidance to the agent
  entry point.
- Documented the existing deterministic crafting harness, its artifact option, and its explicit
  lack of native SDL input/rendering coverage.
- Registered the worktree in the shared workspace inventory and made the pre-push hook executable.

## 2026-09-22 — Complete Agent Smith project documentation

- Replaced scaffold placeholders with the CDDA architecture, data boundaries, conventions, and
  crafting-evaluation workflow.
- Documented the branch's requirement index, focused tests, profiling hooks, and manual harness.
- Made Agent Smith's managed dotfiles trackable while keeping `.agent/project-id` ignored.

## 2026-09-22 — Initial project setup

- Created the Agent Smith project skeleton and LLM instruction files.
