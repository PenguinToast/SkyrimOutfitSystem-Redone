#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CHECK=0
declare -a TARGETS=()

while (($#)); do
    case "$1" in
        --check)
            CHECK=1
            ;;
        *)
            TARGETS+=("$1")
            ;;
    esac
    shift
done

find_clang_format() {
    if command -v clang-format >/dev/null 2>&1; then
        command -v clang-format
        return
    fi

    local tool="/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang-format.exe"
    if [[ -x "$tool" ]]; then
        printf '%s\n' "$tool"
        return
    fi

    echo "clang-format was not found in PATH or the default Visual Studio LLVM install." >&2
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
    find "${REPO_ROOT}/src" -type f \( -name '*.cpp' -o -name '*.h' \) -print0
}

collect_explicit_targets() {
    local target
    for target in "${TARGETS[@]}"; do
        local abs
        abs="$(realpath "${REPO_ROOT}/${target}")"
        if [[ -d "$abs" ]]; then
            find "$abs" -type f \( -name '*.cpp' -o -name '*.h' \) -print0
        elif [[ -f "$abs" ]]; then
            printf '%s\0' "$abs"
        else
            echo "Path not found: ${target}" >&2
            exit 1
        fi
    done
}

CLANG_FORMAT="$(find_clang_format)"
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

for file in "${FILES[@]}"; do
    tool_file="$(to_tool_path "$CLANG_FORMAT" "$file")"
    if ((CHECK)); then
        "$CLANG_FORMAT" --dry-run --Werror --style=file "$tool_file"
    else
        "$CLANG_FORMAT" -i --style=file "$tool_file"
    fi
done

echo "Processed ${#FILES[@]} file(s) with clang-format"
