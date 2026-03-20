# Skyrim Outfit System Redone

`Skyrim Outfit System Redone` is a Skyrim SKSE mod for player vanity outfits.

The goal is to let players keep their real equipped gear for gameplay stats while swapping the
visible appearance to a selected outfit loadout.

The long-term plan is to integrate with a custom fork of Dynamic Armor Variants Extended to handle the
visual-swapping layer of the mod.

The current milestone is the equipment and outfit selector portion of the project:

* enumerate and cache all installed armors and outfits
* provide search, sort, and filter controls for that catalog
* let the player browse outfits separately from individual armor pieces
* prototype an armor-variant workbench UI for currently equipped armor

The browser design and data flow are being shaped with
`https://github.com/Patchu1i/ModExplorerMenu` as a reference for armor/weapon/outfit enumeration,
caching, search behavior, and the native Dear ImGui integration path.

## Requirements
* [XMake](https://xmake.io) [3.0.0+]
* C++23 compiler on Windows (MSVC or Clang-CL)

## Getting Started
```bat
git clone --recurse-submodules git@github.com:PenguinToast/SkyrimOutfitSystem-Redone.git
cd SkyrimOutfitSystem-Redone
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

The current in-game browser is native Dear ImGui rendered directly through the Skyrim D3D11 swapchain.
It no longer depends on Prisma UI or any web frontend bundle.

The current UI focus is a searchable browser for:

* armors
* outfits

Outfits are shown in a dedicated tab because they are collections of individual armor pieces.
The menu is toggled in game with `F6` by default.

The packaged UI font is Ubuntu Regular, bundled with this mod under
`Data/Interface/SkyrimOutfitSystemRedone/fonts/` together with the Ubuntu font license text.
The bundled files come from Canonical's Ubuntu Font Family download archive.

To build from WSL and deploy directly into the local test mod folder:
```bash
./scripts/build-deploy.sh
```

By default this uses `releasedbg` so the deploy includes a `.pdb` alongside the DLL for better crash logs.

For a full clean rebuild:
```bash
./scripts/build-deploy.sh --clean
```

This deploys the plugin to:
`/mnt/f/games/skyrim/modlists/pt_test/mods/skyrim_outfit_system_redone`

To build and package a release mod archive:
```bash
./scripts/build-package.sh
```

This writes a zip under `dist/` named like:
`Skyrim Outfit System Redone v1.2.3.zip`

The package script will fail unless:
* the worktree is clean
* `HEAD` has exactly one tag
* that tag is valid semver

The archive root contains the normal mod payload (`SKSE/`, `Interface/`, etc.)
plus `fomod/info.xml` with mod metadata. Packaging uses `releasedbg`, so the
archive also includes a `.pdb` next to the DLL for symbolized crash logs.

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

## Lint And Format
```bash
./scripts/format.sh
./scripts/format.sh --check
./scripts/lint.sh
./scripts/lint.sh src/Menu.cpp
```

From WSL, `format.sh` prefers Linux `clang-format` when available. `lint.sh` prefers the
Visual Studio LLVM `x64` `clang-tidy.exe` because the generic Visual Studio `Llvm/bin`
binary is a 32-bit build that crashes in this environment.

`lint.sh` also passes `/Y-` to disable MSVC PCH use during linting. xmake's generated
`compile_commands.json` points `clang-tidy` at a PCH path that does not exist, so disabling
PCH is the reliable way to lint these translation units.

## Upgrading Dependencies
```bat
xmake repo --update
xmake require --upgrade
```
