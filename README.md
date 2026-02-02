# Monte Carlo Experiments

This repo is a scratchpad for small Monte Carlo simulations.

## Current project

- **LEC 2026 Versus (League of Legends)**: BO1 single round-robin with top-8 playoffs.
  - Source data lives in `games.txt` (human-editable schedule with results).
  - Simulation code is in `league_mc.cpp` and `main.cpp`.

## Build & run (Windows / MSVC)

This repo includes VS Code tasks for MSVC with OpenMP enabled.

1) Build
- Use the VS Code task: `build (cl x64 via vswhere)`

2) Run
- Use the VS Code launch config: `(Windows) Launch`

## Notes

- The schedule in `games.txt` can be edited to roll back to earlier days.
- Simulations assume 50/50 outcomes for remaining games.
