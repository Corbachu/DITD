#!/usr/bin/env bash
# Safe bash options (omit pipefail for broader shell compatibility)
set -e
set -u

# Normalize KOS base (allow override or default path)
KOS_BASE_DIR="${KOS_BASE:-/opt/toolchains/dc/kos}"

# Source KOS environment
if [ ! -f "$KOS_BASE_DIR/environ.sh" ]; then
	echo "[kos_make] ERROR: KOS environ.sh not found at $KOS_BASE_DIR/environ.sh" >&2
	echo "[kos_make] Hint: export KOS_BASE=/path/to/kos" >&2
	exit 1
fi
# shellcheck source=/dev/null
set +u  # allow KOS scripts to reference unset vars
. "$KOS_BASE_DIR/environ.sh"
set -u

# GLdc auto-build (only if source present)
GLDC_SRC="$KOS_BASE_DIR/addons/GLdc"
GLDC_BUILD="$GLDC_SRC/dcbuild"
GLDC_LIB="$GLDC_BUILD/libGL.a"
GLDC_TOOLCHAIN="$GLDC_SRC/toolchains/Dreamcast.cmake"

if [ -d "$GLDC_SRC" ]; then
	if [ ! -f "$GLDC_LIB" ]; then
		echo "[kos_make] Building GLdc (Release) ..."
		( cd "$GLDC_SRC" \
			&& mkdir -p dcbuild \
			&& cd dcbuild \
			&& cmake -DCMAKE_TOOLCHAIN_FILE="$GLDC_TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .. \
			&& make -j"$(nproc || echo 2)" ) || { echo "[kos_make] GLdc build failed"; exit 1; }
	else
		echo "[kos_make] GLdc present."
	fi
else
	echo "[kos_make] WARNING: GLdc source missing ($GLDC_SRC). Skipping GLdc build."
fi

# Second unconditional GLdc build removed to avoid rebuilding examples every run.

# Configure and build with CMake (Dreamcast toolchain)
# Re-run configure each time so file(GLOB ...) picks up new sources.
BUILD_DIR="${WORKSPACE_BUILD_DIR:-build-dc}"
echo "[kos_make] Configuring CMake in $BUILD_DIR ..."
cmake -B "$BUILD_DIR" -S "$PWD" -DCMAKE_TOOLCHAIN_FILE="$PWD/dreamcast.cmake" -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"

echo "[kos_make] Building target(s) in $BUILD_DIR $*"
cmake --build "$BUILD_DIR" -- -j"$(nproc || echo 2)" "$@"

echo "[kos_make] Done."

# This file is used as a VS Code task init-file (bash --init-file ...).
# Exit explicitly so the task terminates cleanly instead of dropping into an
# interactive prompt where an accidental keypress becomes a command.
exit 0