#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build-package"}
DIST_DIR=${DIST_DIR:-"$ROOT_DIR/dist"}
BUILD_TYPE=${BUILD_TYPE:-Release}

case "$(uname -s)" in
    Darwin) PLATFORM=macos ;;
    Linux) PLATFORM=linux ;;
    *)
        echo "Unsupported platform: $(uname -s)" >&2
        exit 1
        ;;
esac

case "$(uname -m)" in
    x86_64|amd64) ARCH=x86_64 ;;
    arm64|aarch64) ARCH=arm64 ;;
    *) ARCH=$(uname -m) ;;
esac

PACKAGE_NAME="edb-${PLATFORM}-${ARCH}"
PACKAGE_ROOT="$DIST_DIR/$PACKAGE_NAME"
ARCHIVE="$DIST_DIR/$PACKAGE_NAME.tar.gz"

BUILD_DIR="$BUILD_DIR" BUILD_TYPE="$BUILD_TYPE" "$ROOT_DIR/scripts/build.sh"

rm -rf "$PACKAGE_ROOT" "$ARCHIVE"
mkdir -p "$DIST_DIR"
cmake --install "$BUILD_DIR" --prefix "$PACKAGE_ROOT"
tar -C "$DIST_DIR" -czf "$ARCHIVE" "$PACKAGE_NAME"
rm -rf "$PACKAGE_ROOT"

echo "$ARCHIVE"
