#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format is required but was not found in PATH" >&2
    exit 1
fi

# Collect C/C++ sources under edb/, excluding build artifacts.
set --
for path in edb/*.c edb/*.cc edb/*.cpp edb/*.cxx edb/*.h edb/*.hh edb/*.hpp edb/*.hxx; do
    [ -e "$path" ] || continue
    set -- "$@" "$path"
done

if [ "$#" -eq 0 ]; then
    echo "No C/C++ sources found under edb/" >&2
    exit 1
fi

echo "Checking clang-format on $# file(s) with $(clang-format --version)"
clang-format --dry-run --Werror "$@"
echo "clang-format check passed"
