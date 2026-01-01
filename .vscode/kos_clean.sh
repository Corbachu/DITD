#!/usr/bin/env bash
# CMake "clean" for the Dreamcast build directory (CMake context, not raw make).
set -e
set -u

KOS_BASE_DIR="${KOS_BASE:-/opt/toolchains/dc/kos}"

if [ ! -f "$KOS_BASE_DIR/environ.sh" ]; then
	echo "[kos_clean] ERROR: KOS environ.sh not found at $KOS_BASE_DIR/environ.sh" >&2
	echo "[kos_clean] Hint: export KOS_BASE=/path/to/kos" >&2
	exit 1
fi

# shellcheck source=/dev/null
set +u
. "$KOS_BASE_DIR/environ.sh"
set -u

BUILD_DIR="${WORKSPACE_BUILD_DIR:-build-dc}"

if [ ! -d "$BUILD_DIR" ]; then
	echo "[kos_clean] No build directory '$BUILD_DIR' yet; nothing to clean."
	exit 0
fi

echo "[kos_clean] Cleaning in $BUILD_DIR ..."
cmake --build "$BUILD_DIR" --target clean

echo "[kos_clean] Done."
exit 0
