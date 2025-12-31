#!/usr/bin/env bash
set -euo pipefail

# Build a minimal MIL-CD style layout:
# - IP.BIN boot sector
# - 1ST_READ.BIN (scrambled, if tools available)
# - ISO (and CDI if cdi4dc is present)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-dc}"
ELF_PATH="${1:-$BUILD_DIR/Fitd/Fitd.elf}"
TITLE="${DC_TITLE:-FITD}"
COMPANY="${DC_COMPANY:-HOME BREW}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/dc-disc}"
DISC_DIR="$OUT_DIR/disc"

KOS_BASE_DIR="${KOS_BASE:-/opt/toolchains/dc/kos}"

if [[ ! -f "$ELF_PATH" ]]; then
  echo "[make_disc] ERROR: ELF not found: $ELF_PATH" >&2
  echo "[make_disc] Hint: build first (VS Code task: Build)" >&2
  exit 1
fi

mkdir -p "$DISC_DIR"

# Source KOS environment if available (adds sh-elf-* tools to PATH).
if [[ -f "$KOS_BASE_DIR/environ.sh" ]]; then
  # shellcheck source=/dev/null
  set +u
  . "$KOS_BASE_DIR/environ.sh"
  set -u
fi

objcopy_bin() {
  if command -v kos-objcopy >/dev/null 2>&1; then
    kos-objcopy "$@"
  elif command -v sh-elf-objcopy >/dev/null 2>&1; then
    sh-elf-objcopy "$@"
  else
    echo "[make_disc] ERROR: objcopy not found (kos-objcopy / sh-elf-objcopy)" >&2
    exit 1
  fi
}

find_tool() {
  local name="$1"
  local fallback="$2"
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  if [[ -n "$fallback" && -x "$fallback" ]]; then
    echo "$fallback"
    return 0
  fi
  return 1
}

find_kos_util() {
  # Best-effort: locate a tool somewhere under $KOS_BASE/utils.
  local util_name="$1"
  if [[ -d "$KOS_BASE_DIR/utils" ]]; then
    local found
    found=$(find "$KOS_BASE_DIR/utils" -maxdepth 3 -type f \( -name "$util_name" -o -name "${util_name}.elf" \) 2>/dev/null | head -n 1 || true)
    if [[ -n "$found" && -x "$found" ]]; then
      echo "$found"
      return 0
    fi
  fi
  return 1
}

SCRAMBLE_TOOL=""
if SCRAMBLE_TOOL=$(find_tool scramble "$KOS_BASE_DIR/utils/scramble/scramble"); then
  :
elif SCRAMBLE_TOOL=$(find_tool kos-scramble ""); then
  :
elif SCRAMBLE_TOOL=$(find_kos_util scramble); then
  :
fi

MAKEIP_TOOL=""
if MAKEIP_TOOL=$(find_tool makeip "$KOS_BASE_DIR/utils/makeip/makeip"); then
  :
elif MAKEIP_TOOL=$(find_kos_util makeip); then
  :
fi

ISO_TOOL=""
if ISO_TOOL=$(find_tool mkisofs ""); then
  :
elif ISO_TOOL=$(find_tool genisoimage ""); then
  :
fi

CDI_TOOL=""
if CDI_TOOL=$(find_tool cdi4dc ""); then
  :
fi

echo "[make_disc] ELF: $ELF_PATH"

# Produce 1ST_READ.BIN
TMP_1ST="$OUT_DIR/1ST_READ.BIN"
objcopy_bin -O binary "$ELF_PATH" "$TMP_1ST"

if [[ -n "$SCRAMBLE_TOOL" ]]; then
  echo "[make_disc] Scrambling 1ST_READ.BIN (${SCRAMBLE_TOOL})"
  "$SCRAMBLE_TOOL" "$TMP_1ST" "$DISC_DIR/1ST_READ.BIN" >/dev/null 2>&1 || "$SCRAMBLE_TOOL" "$TMP_1ST" "$DISC_DIR/1ST_READ.BIN"
else
  echo "[make_disc] WARNING: scramble tool not found; using raw 1ST_READ.BIN (some emulators may still boot)" >&2
  cp -f "$TMP_1ST" "$DISC_DIR/1ST_READ.BIN"
fi

# Produce IP.BIN
if [[ -n "$MAKEIP_TOOL" ]]; then
  echo "[make_disc] Creating IP.BIN (${MAKEIP_TOOL})"
  # makeip v2.x uses -g/-c for fields; output file is the final arg.
  "$MAKEIP_TOOL" -v -f -g "$TITLE" -c "$COMPANY" "$OUT_DIR/IP.BIN"
else
  echo "[make_disc] ERROR: makeip not found (needed to generate IP.BIN)." >&2
  echo "[make_disc] Expected: $KOS_BASE_DIR/utils/makeip/makeip or makeip in PATH" >&2
  exit 1
fi

if [[ -z "$ISO_TOOL" ]]; then
  echo "[make_disc] WARNING: mkisofs/genisoimage not found; skipping ISO creation." >&2
  echo "[make_disc] Disc layout ready at: $DISC_DIR" >&2
  echo "[make_disc] Boot sector: $OUT_DIR/IP.BIN" >&2
  echo "[make_disc] Program: $DISC_DIR/1ST_READ.BIN" >&2
  exit 0
fi

ISO_OUT="$OUT_DIR/${TITLE}.iso"
# Common MIL-CD mkisofs flags. (Exact values can vary by toolchain/setup.)
"$ISO_TOOL" -quiet -C 0,11702 -V "$TITLE" -G "$OUT_DIR/IP.BIN" -l -r -J \
  -o "$ISO_OUT" "$DISC_DIR"

echo "[make_disc] ISO: $ISO_OUT"

# Optional: CDI
if [[ -n "$CDI_TOOL" ]]; then
  echo "[make_disc] Creating CDI (${CDI_TOOL})"
  "$CDI_TOOL" "$ISO_OUT" "$OUT_DIR/${TITLE}.cdi" || true
  if [[ -f "$OUT_DIR/${TITLE}.cdi" ]]; then
    echo "[make_disc] CDI: $OUT_DIR/${TITLE}.cdi"
  fi
else
  echo "[make_disc] Note: cdi4dc not found; skipping CDI output." >&2
fi

echo "[make_disc] Done. Output dir: $OUT_DIR"
