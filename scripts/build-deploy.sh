#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
CLEAN=0
PLUGIN_NAME="SkyrimVanitySystem"
DEFAULT_MOD_DIR="/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Vanity System"
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

SVS_BUILD_VERSION_STRING="$("${SCRIPT_DIR}/version.sh" --display)"

copy_file() {
    local src="$1"
    local dst="$2"

    if ! cp "$src" "$dst"; then
        echo "Failed to copy ${src} to ${dst}." >&2
        echo "The destination file may be locked by Skyrim, MO2, or another Windows process." >&2
        exit 1
    fi
}

BUILD_ROOT="${REPO_ROOT}/build/runtime"
BUILD_DIR="${BUILD_ROOT}/flat/windows/x64/${MODE}"
PLUGIN_SRC="${BUILD_DIR}/${PLUGIN_NAME}.dll"
PDB_SRC="${BUILD_DIR}/${PLUGIN_NAME}.pdb"
VR_PLUGIN_SRC="${BUILD_ROOT}/vr/windows/x64/${MODE}/${PLUGIN_NAME}.dll"
PLUGIN_DST_DIR="${MOD_DIR}/SKSE/Plugins"
DATA_SRC_DIR="${REPO_ROOT}/data"

BUILD_ARGS=()
if ((CLEAN)); then
    BUILD_ARGS+=("--clean")
fi
BUILD_ARGS+=("$MODE")

"${SCRIPT_DIR}/build.sh" "${BUILD_ARGS[@]}"

if [[ ! -f "$PLUGIN_SRC" ]]; then
    echo "Build succeeded but ${PLUGIN_SRC} was not found." >&2
    exit 1
fi
if [[ ! -f "$VR_PLUGIN_SRC" ]]; then
    echo "Build succeeded but ${VR_PLUGIN_SRC} was not found." >&2
    exit 1
fi

mkdir -p "$PLUGIN_DST_DIR"

copy_file "$PLUGIN_SRC" "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.dll"

if [[ -f "$PDB_SRC" ]]; then
    copy_file "$PDB_SRC" "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb"
else
    rm -f "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb"
fi

if [[ -d "$DATA_SRC_DIR" ]]; then
    cp -R "${DATA_SRC_DIR}/." "${MOD_DIR}/"
fi

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Version ${SVS_BUILD_VERSION_STRING}"
echo "Deployed plugin to ${PLUGIN_DST_DIR}/${PLUGIN_NAME}.dll"
if [[ -f "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb" ]]; then
    echo "Deployed symbols to ${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb"
fi
