#!/usr/bin/env bash
# Development runner: configure, build (Debug) and launch PolyScope.
#
#   ./run.sh            build + run
#   ./run.sh --test     build + run ctest only
#   ./run.sh --clean    wipe build/ first
#
# Environment: QT_PREFIX overrides Qt discovery; BUILD_DIR overrides ./build.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

find_qt_prefix() {
    if [[ -n "${QT_PREFIX:-}" ]]; then
        echo "$QT_PREFIX"
        return
    fi
    local qmake
    for qmake in qmake6 qmake; do
        # A plain "qmake" is often Qt 5; only accept Qt 6.
        if command -v "$qmake" >/dev/null 2>&1 && [[ "$("$qmake" -query QT_VERSION 2>/dev/null)" == 6.* ]]; then
            "$qmake" -query QT_INSTALL_PREFIX
            return
        fi
    done
    echo "Qt 6 not found. Install Qt >= 6.8 or set QT_PREFIX." >&2
    exit 1
}

mode="run"
for arg in "$@"; do
    case "$arg" in
        --test) mode="test" ;;
        --clean)
            # Refuse to delete anything that is not a CMake build tree (BUILD_DIR can be overridden).
            if [[ -d "$BUILD_DIR" && ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
                echo "Refusing to clean $BUILD_DIR: no CMakeCache.txt" >&2
                exit 2
            fi
            rm -rf "$BUILD_DIR" ;;
        -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

QT_PREFIX_DIR="$(find_qt_prefix)"
generator=()
if command -v ninja >/dev/null 2>&1; then
    generator=(-G Ninja)
fi

# macOS ships bash 3.2, where expanding an empty array under `set -u` is an error.
cmake -S "$ROOT" -B "$BUILD_DIR" ${generator[@]+"${generator[@]}"} \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX_DIR"

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake --build "$BUILD_DIR" --parallel "$jobs"

if [[ "$mode" == "test" ]]; then
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD_DIR" --output-on-failure
    exit 0
fi

case "$(uname -s)" in
    Darwin) exe="$BUILD_DIR/PolyScope.app/Contents/MacOS/PolyScope" ;;
    MINGW*|MSYS*|CYGWIN*) exe="$BUILD_DIR/PolyScope.exe" ;;
    *) exe="$BUILD_DIR/PolyScope" ;;
esac

exec "$exe"
