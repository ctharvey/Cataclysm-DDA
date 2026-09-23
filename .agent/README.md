# Cataclysm-DDA-crafting-eval — Agent Docs

This is the agent entry point for the crafting-menu performance evaluation worktree. Read this
file first, then open only the project documents relevant to the task.

## Files

| File | Purpose |
|------|---------|
| `architecture.md` | System design, data flow, tech stack, config/env reference |
| `data_models.md` | Entities, DTOs, enums, converters, repository reference |
| `CHANGELOG.md` | Running log of changes, most recent first |
| `workflows/code_conventions.md` | Language-specific style and formatting rules |

## Repository and Branch

- Repository: `ctharvey/Cataclysm-DDA`, with `CleverRaven/Cataclysm-DDA` as `upstream`.
- This checkout is a linked Git worktree at
  `C:\Users\thron\IdeaProjects\projects\forked\Cataclysm-DDA-crafting-eval`.
- Its intended branch is `eval/glm-5.3-flash-crafting-index-20260908`; do not switch this worktree
  to another branch.
- Upstream's default branch is `master`. Make changes on feature/evaluation branches and do not
  push directly to `master`.
- `crafting-profile-objwin/` and other build outputs are local artifacts. Do not stage or clean
  them as part of documentation or harness work.
- `.agent/project-id` is intentionally ignored and is not required for local scaffold or harness
  use. Menhir registration is deferred.

## Crafting Harness

The existing harness is a deterministic Catch2 runner for crafting-category correctness and
performance. One invocation launches three fresh test processes to emulate opening crafting,
selecting Armor, closing cleanly, and repeating from a cold process. It rejects semantic-count
drift between runs and emits a schema-v2 aggregate with each run and median timings. It is not
native SDL pixel, focus, or input automation.

1. Build `tests/cata_test.exe` with the branch's crafting tests enabled.
2. From this repository root, run `./tools/run_crafting_harness.ps1`.
3. To retain a machine-readable result, pass
   `-OutputPath ./crafting-harness-report.json`; the report includes the scenario name, three
   per-process reports, median timings, the Git revision, and whether tracked files were dirty.

Use an isolated `--user-dir` for native game launches. Keep manual observations separate from the
deterministic harness result, and do not treat a successful harness run as SDL input/rendering
coverage.

## Operating Rules

- Preserve final craftability results; the requirement index may narrow candidates but is not an
  authority for whether a recipe can actually be crafted.
- Keep profiling instrumentation opt-in so normal builds and gameplay remain unchanged.
- Prefer focused `crafting_*` tests and the harness before a broader CDDA build or test run.
- Follow the upstream contribution and formatting guidance linked from the repository `README.md`.

## Maintenance Rules

- Update `data_models.md` when any entity, DTO, or enum changes.
- Update `architecture.md` when adding services, integrations, or structural changes.
- Update the relevant workflow file when operational behavior changes.
- Add a `CHANGELOG.md` entry at the end of every session with meaningful changes.
  Format: `## YYYY-MM-DD — <short description>` with bullet points per file changed.
- Push to git regularly — at minimum at the end of each working session.
- Use conventional commit messages: `feat:`, `fix:`, `refactor:`, `chore:`, `docs:`.
- Only commit files worked on this session. Run `git diff --name-only` and `git status` before staging. Add files explicitly by path — never `git add .` or `git add -A`.
