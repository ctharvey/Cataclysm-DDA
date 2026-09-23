# Changelog — Cataclysm-DDA-crafting-eval

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
