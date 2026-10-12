# ItemIntegrationFramework

Fallout 4 framework project for item cards, shared runtime hooks, and integrations.

## Project layout

- `src/` — C++ implementation and public APIs.
- `修改过的FALLUI的SWF的as文件（信息卡）/` — existing AS3 source for the FallUI item card integration.
- `data/` — project runtime image. Generated DLLs and SWFs are ignored by Git; other runtime assets remain available locally.
- `F4SE/`, `Interface/`, and `PrismaUI_F4/` — retained legacy source/runtime mirrors from the previous workspace.
- `mod.json` — project metadata used by the workspace.

`project.json` is retained as legacy metadata from the previous workspace. New workspace tooling uses `mod.json`.

## Build

Run the project wrapper, which delegates to the workspace-locked C++ build entry:

```powershell
.\scripts\build.ps1 -Configuration releasedbg
```

The build output stays under this project's `build/`. Copy verified runtime output into `data/` only as a separate staging step. Do not copy files into the game directory or MO2 from this project script.

## Git

This project is an independent Git repository. `main` follows the GitHub repository; `local` contains the imported second-generation workspace snapshot. GitHub branches are retained as `origin/*` remote-tracking branches. Publishing remains manual.