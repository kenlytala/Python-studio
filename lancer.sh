#!/usr/bin/env sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
# A separate build avoids reusing the old release objects or OpenGL 3.3 cache.
# Always update sources and the raylib startup guards before executing.
build_dir="${PRISME_BUILD_DIR:-build-1.21.0}"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1
cmake --build "$build_dir" --parallel
exec "$build_dir/python-studio" "$@"
