# Skyrim Outfit System NG

`Skyrim Outfit System NG` is a Skyrim SKSE mod for player vanity outfits.

The goal is to let players keep their real equipped gear for gameplay stats while swapping the
visible appearance to a selected outfit loadout.

The long-term plan is to integrate with a custom fork of Dynamic Armor Variants to handle the
visual-swapping layer of the mod.

The current milestone is the equipment and outfit selector portion of the project:

* enumerate and cache all installed armors, weapons, and outfits
* provide search, sort, and filter controls for that catalog
* let the player browse outfits separately from individual gear pieces

The browser design and data flow are being shaped with
`https://github.com/Patchu1i/ModExplorerMenu` as a reference for armor/weapon/outfit enumeration,
caching, search behavior, and the native Dear ImGui integration path.

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

The current in-game browser is native Dear ImGui rendered directly through the Skyrim D3D11 swapchain.
It no longer depends on Prisma UI or any web frontend bundle.

The current UI focus is a searchable browser for:

* armors
* weapons
* outfits

Outfits are shown in a dedicated tab because they are collections of individual gear pieces.
The menu is toggled in game with `F3`.

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
