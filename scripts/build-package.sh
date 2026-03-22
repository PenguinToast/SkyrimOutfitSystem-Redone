#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
PLUGIN_NAME="SkyrimOutfitSystemRedone"
MOD_NAME="Skyrim Outfit System Redone"
MOD_AUTHOR="PenguinToast"
MOD_DESCRIPTION="SKSE plugin for player vanity outfits with a native Dear ImGui browser."
DATA_SRC_DIR="${REPO_ROOT}/data"
FOMOD_SRC_DIR="${REPO_ROOT}/fomod"
DIST_DIR="${REPO_ROOT}/dist"
STAGE_DIR="${DIST_DIR}/.stage"
BUILD_ROOT="${REPO_ROOT}/build/package"

while (($#)); do
    case "$1" in
        --clean)
            rm -rf "${REPO_ROOT}/build" "${REPO_ROOT}/.xmake" "${DIST_DIR}"
            ;;
        *)
            echo "Usage: $0 [--clean]" >&2
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
    echo "wslpath is required to convert paths for Windows tools." >&2
    exit 1
fi

normalize_repo_url() {
    local remote_url="$1"
    if [[ "${remote_url}" =~ ^git@github\.com:(.+)/(.+)\.git$ ]]; then
        printf 'https://github.com/%s/%s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    elif [[ "${remote_url}" =~ ^https://github\.com/(.+)/(.+)\.git$ ]]; then
        printf 'https://github.com/%s/%s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    else
        printf '%s\n' "${remote_url}"
    fi
}

REMOTE_URL="$(normalize_repo_url "$(git -C "${REPO_ROOT}" remote get-url upstream 2>/dev/null || git -C "${REPO_ROOT}" remote get-url origin)")"
SOSR_BUILD_VERSION="$("${SCRIPT_DIR}/version.sh" --numeric)"
SOSR_BUILD_VERSION_STRING="$("${SCRIPT_DIR}/version.sh" --display)"
VERSION="${SOSR_BUILD_VERSION_STRING}"
ARCHIVE_NAME="${MOD_NAME} v${VERSION}.zip"
ARCHIVE_PATH="${DIST_DIR}/${ARCHIVE_NAME}"

WIN_REPO_ROOT="$(wslpath -w "${REPO_ROOT}")"
WIN_ARCHIVE_PATH="$(wslpath -w "${ARCHIVE_PATH}")"
WIN_STAGE_DIR="$(wslpath -w "${STAGE_DIR}")"

build_variant() {
    local variant_name="$1"
    shift
    local skyrim_se="$1"
    local skyrim_ae="$2"
    local skyrim_vr="$3"
    local variant_build_dir="${BUILD_ROOT}/${variant_name}"
    local variant_plugin_src="${variant_build_dir}/windows/x64/${MODE}/${PLUGIN_NAME}.dll"
    local variant_pdb_src="${variant_build_dir}/windows/x64/${MODE}/${PLUGIN_NAME}.pdb"
    local win_variant_build_dir
    win_variant_build_dir="$(wslpath -w "${variant_build_dir}")"

    local powershell_cmd="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
\$env:SOSR_BUILD_VERSION = '$SOSR_BUILD_VERSION'
\$env:SOSR_BUILD_VERSION_STRING = '$SOSR_BUILD_VERSION_STRING'
xmake f -y --builddir='$win_variant_build_dir' --skyrim_se=${skyrim_se} --skyrim_ae=${skyrim_ae} --skyrim_vr=${skyrim_vr} -m '$MODE'
xmake build -y
"
    powershell.exe -NoProfile -Command "${powershell_cmd}"

    if [[ ! -f "${variant_plugin_src}" ]]; then
        echo "Build succeeded but ${variant_plugin_src} was not found." >&2
        exit 1
    fi

    printf '%s\n%s\n' "${variant_plugin_src}" "${variant_pdb_src}"
}

rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}/Data" \
         "${STAGE_DIR}/SkyrimSE/SKSE/Plugins" \
         "${STAGE_DIR}/SkyrimVR/SKSE/Plugins" \
         "${STAGE_DIR}/fomod"

if [[ -d "${DATA_SRC_DIR}" ]]; then
    cp -R "${DATA_SRC_DIR}/." "${STAGE_DIR}/Data/"
fi

if [[ -d "${FOMOD_SRC_DIR}" ]]; then
    cp -R "${FOMOD_SRC_DIR}/." "${STAGE_DIR}/fomod/"
fi

mapfile -t flat_outputs < <(build_variant flat y y n)
cp "${flat_outputs[0]}" "${STAGE_DIR}/SkyrimSE/SKSE/Plugins/${PLUGIN_NAME}.dll"
if [[ -f "${flat_outputs[1]}" ]]; then
    cp "${flat_outputs[1]}" "${STAGE_DIR}/SkyrimSE/SKSE/Plugins/${PLUGIN_NAME}.pdb"
fi

mapfile -t vr_outputs < <(build_variant vr n n y)
cp "${vr_outputs[0]}" "${STAGE_DIR}/SkyrimVR/SKSE/Plugins/${PLUGIN_NAME}.dll"
if [[ -f "${vr_outputs[1]}" ]]; then
    cp "${vr_outputs[1]}" "${STAGE_DIR}/SkyrimVR/SKSE/Plugins/${PLUGIN_NAME}.pdb"
fi

python3 - <<'PY' "${STAGE_DIR}/fomod"
from pathlib import Path
import sys

fomod_dir = Path(sys.argv[1])
if fomod_dir.is_dir():
    for xml_path in fomod_dir.glob("*.xml"):
        raw = xml_path.read_bytes()
        text = None
        for encoding in ("utf-8-sig", "utf-16", "utf-16-le", "utf-16-be"):
            try:
                text = raw.decode(encoding)
                break
            except UnicodeDecodeError:
                continue
        if text is None:
            raise SystemExit(f"Could not decode {xml_path}")
        xml_path.write_text(text.lstrip("\ufeff"), encoding="utf-8", newline="\n")
PY

cat > "${STAGE_DIR}/fomod/info.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<fomod>
  <Name>${MOD_NAME}</Name>
  <Author>${MOD_AUTHOR}</Author>
  <Version>${VERSION}</Version>
  <Website>${REMOTE_URL}</Website>
  <Description>${MOD_DESCRIPTION}</Description>
</fomod>
EOF

mkdir -p "${DIST_DIR}"
rm -f "${ARCHIVE_PATH}"

POWERSHELL_ZIP_CMD="
\$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath '$WIN_ARCHIVE_PATH') {
  Remove-Item -LiteralPath '$WIN_ARCHIVE_PATH' -Force
}
Compress-Archive -Path '$WIN_STAGE_DIR\\*' -DestinationPath '$WIN_ARCHIVE_PATH' -CompressionLevel Optimal
"
powershell.exe -NoProfile -Command "${POWERSHELL_ZIP_CMD}"

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Packaged mod to ${ARCHIVE_PATH}"
