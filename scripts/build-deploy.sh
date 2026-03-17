#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="release"
CLEAN=0
PLUGIN_NAME="SkyrimOutfitSystemNG"
VIEW_NAME="skyrim-outfit-system-ng"
DEFAULT_MOD_DIR="/mnt/f/games/skyrim/modlists/pt_test/mods/skyrim_outfit_system_ng"
MOD_DIR="${MOD_DIR:-$DEFAULT_MOD_DIR}"

while (($#)); do
    case "$1" in
        --clean)
            CLEAN=1
            ;;
        release|debug|releasedbg)
            MODE="$1"
            ;;
        *)
            echo "Usage: $0 [--clean] [release|debug|releasedbg]" >&2
            exit 2
            ;;
    esac
    shift
done

if ! command -v powershell.exe >/dev/null 2>&1; then
    echo "powershell.exe is required to invoke the Windows toolchain from WSL." >&2
    exit 1
fi

if ! command -v wslpath >/dev/null 2>&1; then
    echo "wslpath is required to convert the repo path for Windows xmake." >&2
    exit 1
fi

copy_file() {
    local src="$1"
    local dst="$2"

    if ! cp "$src" "$dst"; then
        echo "Failed to copy ${src} to ${dst}." >&2
        echo "The destination file may be locked by Skyrim, MO2, or another Windows process." >&2
        exit 1
    fi
}

WIN_REPO_ROOT="$(wslpath -w "$REPO_ROOT")"
BUILD_DIR="${REPO_ROOT}/build/windows/x64/${MODE}"
PLUGIN_SRC="${BUILD_DIR}/${PLUGIN_NAME}.dll"
PDB_SRC="${BUILD_DIR}/${PLUGIN_NAME}.pdb"
PLUGIN_DST_DIR="${MOD_DIR}/SKSE/Plugins"
VIEW_DST_DIR="${MOD_DIR}/PrismaUI/views/${VIEW_NAME}"

if ((CLEAN)); then
    rm -rf "${REPO_ROOT}/build" "${REPO_ROOT}/.xmake"
fi

POWERSHELL_CMD="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
xmake f -c
xmake f -m '$MODE'
xmake build -y
"
powershell.exe -NoProfile -Command "$POWERSHELL_CMD"

if [[ ! -f "$PLUGIN_SRC" ]]; then
    echo "Build succeeded but ${PLUGIN_SRC} was not found." >&2
    exit 1
fi

mkdir -p "$PLUGIN_DST_DIR" "$VIEW_DST_DIR"

copy_file "$PLUGIN_SRC" "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.dll"
copy_file "${REPO_ROOT}/view/index.html" "${VIEW_DST_DIR}/index.html"

if [[ -f "$PDB_SRC" ]]; then
    copy_file "$PDB_SRC" "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb"
else
    rm -f "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb"
fi

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Deployed plugin to ${PLUGIN_DST_DIR}/${PLUGIN_NAME}.dll"
echo "Deployed Prisma view to ${VIEW_DST_DIR}/index.html"
