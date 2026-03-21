# Skyrim Outfit System Redone

`Skyrim Outfit System Redone` is a Skyrim SKSE mod for player-facing vanity outfit management.

It lets the player keep their real equipped gear for gameplay while changing the visible appearance through a native in-game browser and variant workbench.

Requires https://github.com/PenguinToast/DynamicArmorVariants-Extended to function.

100% vibecoded with Codex, **heavily** inspired by [Modex](https://github.com/patchulidev/ModExplorerMenu) for catalog/UI.

## Features
- Snappy ImGui interface for managing armor appearance overrides
- Searchable catalog for:
  - individual armor pieces
  - outfits
  - Modex kits
- Persistent favorites system
- Preview mode for selected gear, outfits, and kits.
- Integration with Modex kits - creation from equipped gear or active overrides
- SKSE save/load for overrides.
- Modex-style theme loading, fade behavior, and smooth scrolling.

## Requirements
- [XMake](https://xmake.io) 3.0.0+
- C++23 compiler on Windows (MSVC or Clang-CL)

## Getting Started
```bash
git clone --recurse-submodules git@github.com:PenguinToast/SkyrimOutfitSystem-Redone.git
cd SkyrimOutfitSystem-Redone
```

If you cloned without submodules:
```bash
git submodule update --init --recursive
```

## Build
From Windows:

```bat
xmake build
```

This generates output under `build/windows/` in the project root.

From WSL, to build and deploy directly into the local test mod folder:

```bash
./scripts/build-deploy.sh
```

By default this uses `releasedbg`, so the deploy includes a `.pdb` alongside the DLL for better crash logs.

For a full clean rebuild:

```bash
./scripts/build-deploy.sh --clean
```

By default this deploys to:
`/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Outfit System Redone`

## Build And Package
To build a release mod archive:

```bash
./scripts/build-package.sh
```

This writes a zip under `dist/` named like:
`Skyrim Outfit System Redone v1.0.0.zip`

The package script will fail unless:
- the worktree is clean
- `HEAD` has exactly one tag
- that tag is valid semver

The archive root contains the normal mod payload plus `fomod/info.xml`.
Packaging uses `releasedbg`, so the archive also includes a `.pdb` next to the DLL.

## Settings And Data
- Runtime settings are stored under:
  - `Data/SKSE/Plugins/SkyrimOutfitSystemRedone/settings.json`
- Favorites are stored separately in:
  - `Data/SKSE/Plugins/SkyrimOutfitSystemRedone/favorites.json`
- Bundled UI assets are under:
  - `Data/Interface/SkyrimOutfitSystemRedone/`

## Modex Integration
- Modex kits are loaded from:
  - `Data/Interface/Modex/user/kits`
- SOSR can browse kits, preview them, and add them into the workbench.
- SOSR can also create new Modex kits from equipped gear or active overrides.

## Lint And Format
```bash
./scripts/format.sh
./scripts/format.sh --check
./scripts/lint.sh
./scripts/lint.sh src/ui/Menu.cpp
```

From WSL, `format.sh` prefers Linux `clang-format` when available. `lint.sh` prefers the
Visual Studio LLVM `x64` `clang-tidy.exe` because the generic Visual Studio `Llvm/bin`
binary is a 32-bit build that crashes in this environment.

`lint.sh` also passes `/Y-` to disable MSVC PCH use during linting. xmake's generated
`compile_commands.json` points `clang-tidy` at a PCH path that does not exist, so disabling
PCH is the reliable way to lint these translation units.

## Project Generation
For Visual Studio:

```bat
xmake project -k vsxmake
```

For `clangd` or other LSP tooling:

```bat
xmake project -k compile_commands
```
