# ItemIntegrationFramework

Fallout 4 framework project for item cards, shared runtime hooks, and integrations.

## Project layout

- `src/` — C++ implementation and public APIs.
- `as3/` — existing AS3 source for the FallUI item card integration.
- `data/` — project runtime image. Generated DLLs and SWFs are ignored by Git; other runtime assets remain available locally.
- `archive/legacy-runtime-root/` — preserved legacy runtime mirrors from the previous workspace; current runtime staging is under `data/`.
- `mod.json` — project metadata used by the workspace.

`project.json` is retained as legacy metadata from the previous workspace. New workspace tooling uses `mod.json`.

## Build

Run the project wrapper, which delegates to the workspace-locked C++ build entry:

```powershell
.\scripts\build.ps1 -Configuration releasedbg
```

The build output stays under this project's `build/`. Copy verified runtime output into `data/` only as a separate staging step. Do not copy files into the game directory or MO2 from this project script.

## Git

This project is an independent Git repository. Develop and test on `main`, then publish verified changes to `origin/main`. Use a Git worktree for parallel experiments; keep the workspace-only `mod.json` out of commits.
