#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
CLEAN=0
PLUGIN_NAME="SkyrimVanitySystem"
BUILD_ROOT="${REPO_ROOT}/build/runtime"
BUILD_TARGET="flat"

while (($#)); do
    case "$1" in
        --clean)
            CLEAN=1
            ;;
        --all)
            BUILD_TARGET="all"
            ;;
        --vr)
            BUILD_TARGET="vr"
            ;;
        release|debug|releasedbg)
            MODE="$1"
            ;;
        *)
            echo "Usage: $0 [--clean] [--all|--vr] [release|debug|releasedbg]" >&2
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

SVS_BUILD_VERSION="$("${SCRIPT_DIR}/version.sh" --numeric)"
SVS_BUILD_VERSION_STRING="$("${SCRIPT_DIR}/version.sh" --display)"
WIN_REPO_ROOT="$(wslpath -w "${REPO_ROOT}")"

if ((CLEAN)); then
    rm -rf "${REPO_ROOT}/build" "${REPO_ROOT}/.xmake"
fi

build_variant() {
    local variant_name="$1"
    shift
    local skyrim_se="$1"
    local skyrim_ae="$2"
    local skyrim_vr="$3"
    local variant_build_dir="${BUILD_ROOT}/${variant_name}"
    local win_variant_build_dir
    win_variant_build_dir="$(wslpath -w "${variant_build_dir}")"

    local powershell_cmd="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
\$env:SVS_BUILD_VERSION = '$SVS_BUILD_VERSION'
\$env:SVS_BUILD_VERSION_STRING = '$SVS_BUILD_VERSION_STRING'
xmake f -y --builddir='$win_variant_build_dir' --skyrim_se=${skyrim_se} --skyrim_ae=${skyrim_ae} --skyrim_vr=${skyrim_vr} -m '$MODE'
xmake build -y
"
    powershell.exe -NoProfile -Command "${powershell_cmd}"

    local plugin_src="${variant_build_dir}/windows/x64/${MODE}/${PLUGIN_NAME}.dll"
    if [[ ! -f "${plugin_src}" ]]; then
        echo "Build succeeded but ${plugin_src} was not found." >&2
        exit 1
    fi
}

case "$BUILD_TARGET" in
    flat)
        build_variant flat y y n
        ;;
    vr)
        build_variant vr n n y
        ;;
    all)
        build_variant flat y y n
        build_variant vr n n y
        ;;
esac

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Version ${SVS_BUILD_VERSION_STRING}"
if [[ "$BUILD_TARGET" == "flat" || "$BUILD_TARGET" == "all" ]]; then
    echo "Flat build: ${BUILD_ROOT}/flat/windows/x64/${MODE}/${PLUGIN_NAME}.dll"
fi
if [[ "$BUILD_TARGET" == "vr" || "$BUILD_TARGET" == "all" ]]; then
    echo "VR build: ${BUILD_ROOT}/vr/windows/x64/${MODE}/${PLUGIN_NAME}.dll"
fi
