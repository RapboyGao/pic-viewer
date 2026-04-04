#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
APP_PATH="${BUILD_DIR}/pic-viewer"

if [[ ! -x "${APP_PATH}" ]]; then
  echo "==> App not found, building first"
  "${ROOT_DIR}/scripts/build.sh"
fi

exec "${APP_PATH}" "$@"
