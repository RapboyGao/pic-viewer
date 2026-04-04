#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
CMAKE_GENERATOR="${CMAKE_GENERATOR:-}"
QT_PREFIX="${QT_PREFIX:-}"
RUN_TESTS="${RUN_TESTS:-1}"

if [[ -z "${QT_PREFIX}" ]]; then
  if command -v brew >/dev/null 2>&1 && brew --prefix qt >/dev/null 2>&1; then
    QT_PREFIX="$(brew --prefix qt)"
  elif [[ -n "${Qt6_DIR:-}" ]]; then
    QT_PREFIX="$(cd "${Qt6_DIR}/../.." && pwd)"
  fi
fi

if [[ -z "${QT_PREFIX}" ]]; then
  echo "error: Qt prefix not found. Set QT_PREFIX or Qt6_DIR first." >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}"

CMAKE_ARGS=(
  -S "${ROOT_DIR}"
  -B "${BUILD_DIR}"
  "-DCMAKE_PREFIX_PATH=${QT_PREFIX}"
)

if [[ -n "${CMAKE_GENERATOR}" ]]; then
  CMAKE_ARGS+=(-G "${CMAKE_GENERATOR}")
fi

echo "==> Configuring with Qt at: ${QT_PREFIX}"
cmake "${CMAKE_ARGS[@]}"

echo "==> Building"
cmake --build "${BUILD_DIR}" --parallel

if [[ "${RUN_TESTS}" == "1" ]]; then
  echo "==> Running tests"
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

echo "==> Build complete: ${BUILD_DIR}/pic-viewer"
