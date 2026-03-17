# Skyrim Outfit System NG

This is the initial PrismaUI + CommonLibSSE-NG scaffold for `Skyrim Outfit System NG`.

## Requirements
* [XMake](https://xmake.io) [3.0.0+]
* C++23 compiler on Windows (MSVC or Clang-CL)

## Getting Started
```bat
git clone --recurse-submodules git@github.com:PenguinToast/skyrim-outfit-system-ng.git
cd skyrim-outfit-system-ng
```

If you cloned without submodules:
```bat
git submodule update --init --recursive
```

## Build
Build from Windows, not WSL:
```bat
xmake build
```

This generates output under `build/windows/` in the project root.

The Prisma UI frontend now lives under `frontend/` and is built with Vite, Svelte, and TypeScript.
Its production output is generated into `view/`.

To work on the frontend directly:
```bash
cd frontend
npm install
npm run dev
```

To build only the frontend bundle:
```bash
cd frontend
npm run build
```

To build from WSL and deploy directly into the local test mod folder:
```bash
./scripts/build-deploy.sh
```

For a full clean rebuild:
```bash
./scripts/build-deploy.sh --clean
```

This deploys the plugin to:
`/mnt/f/games/skyrim/modlists/pt_test/mods/skyrim_outfit_system_ng`

and deploys the built Prisma view to:
`/mnt/f/games/skyrim/modlists/pt_test/mods/skyrim_outfit_system_ng/PrismaUI/views/skyrim-outfit-system-ng`

## Optional Output Paths
You can redirect install output with either of these environment variables:

* `XSE_TES5_MODS_PATH`
* `XSE_TES5_GAME_PATH`

## Project Generation
For Visual Studio:
```bat
xmake project -k vsxmake
```

For `clangd` or other LSP tooling:
```bat
xmake project -k compile_commands
```

## Upgrading Dependencies
```bat
xmake repo --update
xmake require --upgrade
```
