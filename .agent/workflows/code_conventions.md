# Code Conventions — Cataclysm-DDA-crafting-eval

## Style

- Follow `doc/c++/CODE_STYLE.md` and the repository `.astylerc`; upstream expects Artistic Style
  3.1 output.
- C++ uses four-space indentation and the project's spaced-parenthesis style, for example
  `if( condition )`.
- Keep lines within the formatter's 100-column target where practical.
- Do not hand-format generated or third-party sources.
- Preserve JSON formatting and validate JSON changes with the repository tools.

## Naming

- C++ files, functions, and variables generally use `snake_case`; types follow the conventions of
  their existing domain.
- Keep declarations and definitions in matching `.h`/`.cpp` files.
- Use typed/string IDs and existing factories for JSON-defined objects.
- Test cases should describe behavior and carry focused Catch2 tags so they can run in isolation.

## File Organization

- Production C++ belongs in `src/`; tests belong in `tests/`.
- Static content belongs in the matching subtree under `data/`; do not embed content tables in C++
  when an established JSON schema exists.
- Developer utilities and repeatable harnesses belong in `tools/` or `build-scripts/` according to
  existing neighboring scripts.
- Keep performance instrumentation opt-in. Normal game builds and behavior must not depend on a
  profiling run or its output directory.

## Testing

- Build and run the normal test suite with the selected Make/CMake configuration; the resulting
  Catch2 binary is `tests/cata_test` for Make builds or under the CMake build directory.
- Run focused tests by name or tag before the full suite. Always pass a disposable `--user-dir`
  when a test or game run can create saves/configuration.
- For this branch, use `tools/run_crafting_harness.ps1` for the repeatable crafting scenarios and
  retain the focused `crafting_*` tests as correctness gates around optimizations.
- Run `make astyle-check` for C++ style and the relevant JSON validators for content changes.
- Performance changes need both semantic assertions and measurements; faster output is not useful
  if candidate filtering changes which recipes are ultimately craftable.
