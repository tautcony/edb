#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build-tests"}
BUILD_TYPE=${BUILD_TYPE:-Debug}

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTING=ON
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
