#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build-lint"}

for tool in cmake clang-tidy cppcheck; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool is required but was not found in PATH" >&2
        exit 1
    fi
done

if [ -n "${CLANGXX:-}" ]; then
    :
elif command -v brew >/dev/null 2>&1 && brew --prefix llvm >/dev/null 2>&1; then
    CLANGXX="$(brew --prefix llvm)/bin/clang++"
else
    CLANGXX=$(command -v clang++ || true)
fi

if [ -z "$CLANGXX" ] || [ ! -x "$CLANGXX" ]; then
    echo "clang++ is required to match clang-tidy's compiler headers" >&2
    exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DBUILD_TESTING=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_CXX_COMPILER="$CLANGXX" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY

set -- \
    "$ROOT_DIR/edb/EDBInterface.cpp" \
    "$ROOT_DIR/edb/EDBUtils.cpp" \
    "$ROOT_DIR/edb/main.cpp" \
    "$ROOT_DIR/tests/EDBUtilsTests.cpp" \
    "$ROOT_DIR/tests/EDBInterfaceTests.cpp" \
    "$ROOT_DIR/tests/EDBTransportFactoryStub.cpp"

echo "Running clang-tidy"
clang-tidy -p "$BUILD_DIR" "$@"

echo "Running cppcheck"
cppcheck \
    --project="$BUILD_DIR/compile_commands.json" \
    --enable=warning,performance,portability \
    --inline-suppr \
    --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    --suppress='*:*_deps*' \
    --suppress=virtualCallInConstructor \
    --suppress='unknownMacro:*EDBWinReg.cpp' \
    --quiet

echo "Static analysis passed"
