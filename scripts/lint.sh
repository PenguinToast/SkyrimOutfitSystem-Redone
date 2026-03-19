#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
declare -a TARGETS=()

while (($#)); do
    case "$1" in
        release|debug|releasedbg)
            MODE="$1"
            ;;
        *)
            TARGETS+=("$1")
            ;;
    esac
    shift
done

if ! command -v powershell.exe >/dev/null 2>&1; then
    echo "powershell.exe is required to generate compile_commands.json from WSL." >&2
    exit 1
fi

if ! command -v wslpath >/dev/null 2>&1; then
    echo "wslpath is required to convert repo paths for Windows tools." >&2
    exit 1
fi

find_clang_tidy() {
    if command -v clang-tidy >/dev/null 2>&1; then
        command -v clang-tidy
        return
    fi

    local tool="/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang-tidy.exe"
    if [[ -x "$tool" ]]; then
        printf '%s\n' "$tool"
        return
    fi

    echo "clang-tidy was not found in PATH or the default Visual Studio LLVM install." >&2
    exit 1
}

to_tool_path() {
    local tool="$1"
    local path="$2"
    if [[ "$tool" == *.exe ]]; then
        wslpath -w "$path"
    else
        printf '%s\n' "$path"
    fi
}

collect_default_targets() {
    find "${REPO_ROOT}/src" -type f -name '*.cpp' -print0
}

collect_explicit_targets() {
    local target
    for target in "${TARGETS[@]}"; do
        local abs
        abs="$(realpath "${REPO_ROOT}/${target}")"
        if [[ -d "$abs" ]]; then
            find "$abs" -type f -name '*.cpp' -print0
        elif [[ -f "$abs" ]]; then
            printf '%s\0' "$abs"
        else
            echo "Path not found: ${target}" >&2
            exit 1
        fi
    done
}

WIN_REPO_ROOT="$(wslpath -w "$REPO_ROOT")"
POWERSHELL_CMD="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
xmake f -y -c
xmake f -y -m '$MODE'
xmake project -k compile_commands
"
powershell.exe -NoProfile -Command "$POWERSHELL_CMD" >/dev/null

CLANG_TIDY="$(find_clang_tidy)"
declare -a FILES=()

if ((${#TARGETS[@]})); then
    while IFS= read -r -d '' file; do
        FILES+=("$file")
    done < <(collect_explicit_targets)
else
    while IFS= read -r -d '' file; do
        FILES+=("$file")
    done < <(collect_default_targets)
fi

if ((${#FILES[@]} == 0)); then
    echo "No source files found." >&2
    exit 1
fi

PROJECT_PATH="$(to_tool_path "$CLANG_TIDY" "$REPO_ROOT")"
for file in "${FILES[@]}"; do
    tool_file="$(to_tool_path "$CLANG_TIDY" "$file")"
    "$CLANG_TIDY" -p "$PROJECT_PATH" --header-filter='^(src|include)/' "$tool_file"
done

echo "Processed ${#FILES[@]} translation unit(s) with clang-tidy"
