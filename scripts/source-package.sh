#!/usr/bin/env bash
set -euo pipefail

build_dir="_build/source-package"
fetch_dir="vendor-source-package"
vendor_dir="vendor"

rm -rf "$build_dir" "$fetch_dir"

if [[ -e "$vendor_dir" ]]; then
  echo "Refusing to overwrite existing $vendor_dir/"
  echo "Move it aside or delete it manually first."
  exit 1
fi

cmake -S . -B "$build_dir" \
  -DFETCHCONTENT_BASE_DIR="$PWD/$fetch_dir" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "$build_dir"

mkdir -p "$vendor_dir"

cp -a "$fetch_dir/imgui-src" "$vendor_dir/imgui"
cp -a "$fetch_dir/sdl3-src" "$vendor_dir/SDL3"
cp -a "$fetch_dir/io_platform-src" "$vendor_dir/IO_platform"

rm -Rf "$fetch_dir"

cmake --build "$build_dir" --target package_source

echo
echo "Source packages created under:"
find "$build_dir" -maxdepth 1 \( -name "*.tar.gz" -o -name "*.zip" \) -print
