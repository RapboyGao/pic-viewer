#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v xcode-select >/dev/null 2>&1 || ! xcode-select -p >/dev/null 2>&1; then
  echo "warning: Xcode Command Line Tools are not installed."
  echo "Install them with: xcode-select --install"
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "==> Homebrew not found, installing it first"
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

if ! command -v brew >/dev/null 2>&1; then
  if [[ -x /opt/homebrew/bin/brew ]]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
  elif [[ -x /usr/local/bin/brew ]]; then
    eval "$(/usr/local/bin/brew shellenv)"
  fi
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "error: Homebrew is still unavailable after installation." >&2
  exit 1
fi

echo "==> Installing development tools"
brew update
brew install cmake qt libheif libraw jpeg-turbo pkg-config ninja

QT_PREFIX="$(brew --prefix qt)"
echo "==> Development environment ready"
echo
echo "Qt prefix: ${QT_PREFIX}"
echo
echo "For convenience in new shells, add these lines to your shell profile:"
echo "export PATH=\"${QT_PREFIX}/bin:\$PATH\""
echo "export PKG_CONFIG_PATH=\"${QT_PREFIX}/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo
echo "Build with:"
echo "  ./scripts/build.sh"
