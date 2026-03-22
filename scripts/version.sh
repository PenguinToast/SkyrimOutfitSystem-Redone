#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="display"
while (($#)); do
    case "$1" in
        --numeric)
            MODE="numeric"
            ;;
        --display)
            MODE="display"
            ;;
        *)
            echo "Usage: $0 [--numeric|--display]" >&2
            exit 2
            ;;
    esac
    shift
done

SEMVER_REGEX='^v?([0-9]+)\.([0-9]+)\.([0-9]+)$'

find_latest_release_tag() {
    local tag
    while IFS= read -r tag; do
        if [[ "${tag}" =~ ${SEMVER_REGEX} ]]; then
            printf '%s\n' "${tag}"
            return 0
        fi
    done < <(git -C "${REPO_ROOT}" tag --sort=-version:refname)
    return 1
}

HEAD_TAG=""
mapfile -t HEAD_TAGS < <(git -C "${REPO_ROOT}" tag --points-at HEAD --sort=-creatordate)
for tag in "${HEAD_TAGS[@]}"; do
    if [[ "${tag}" =~ ${SEMVER_REGEX} ]]; then
        HEAD_TAG="${tag}"
        break
    fi
done

if [[ -n "${HEAD_TAG}" ]]; then
    BASE_VERSION="${HEAD_TAG#v}"
    DISPLAY_VERSION="${BASE_VERSION}"
else
    LAST_TAG="$(find_latest_release_tag || true)"
    if [[ -z "${LAST_TAG}" ]]; then
        BASE_VERSION="0.0.0"
    else
        BASE_VERSION="${LAST_TAG#v}"
    fi

    SHORT_SHA="$(git -C "${REPO_ROOT}" rev-parse --short HEAD)"
    DIRTY_SUFFIX=""
    if [[ -n "$(git -C "${REPO_ROOT}" status --porcelain)" ]]; then
        DIRTY_SUFFIX=".dirty"
    fi
    DISPLAY_VERSION="${BASE_VERSION}-dev+${SHORT_SHA}${DIRTY_SUFFIX}"
fi

if [[ "${MODE}" == "numeric" ]]; then
    printf '%s\n' "${BASE_VERSION}"
else
    printf '%s\n' "${DISPLAY_VERSION}"
fi
