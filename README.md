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

Move `view/index.html` into your mod's PrismaUI view folder as:
`<YourModFolder>/PrismaUI/views/skyrim-outfit-system-ng/index.html`

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
