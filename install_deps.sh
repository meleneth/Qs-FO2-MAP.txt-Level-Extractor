#!/usr/bin/env bash
set -euo pipefail

if ! command -v pacman >/dev/null 2>&1; then
  echo "install_deps.sh must be run from an MSYS2 shell with pacman on PATH." >&2
  exit 1
fi

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
  echo "This project is configured for the MSYS2 UCRT64 environment." >&2
  echo "Open an UCRT64 shell, then run ./install_deps.sh again." >&2
  exit 1
fi

pacman -S --needed --noconfirm \
  git \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-sdl3 \
  mingw-w64-ucrt-x86_64-catch
