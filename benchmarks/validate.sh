#!/usr/bin/env bash
# Validates that the hybrid MPI + OpenMP solver produces byte-identical
# output to the serial path (mpirun -n 1). Exits 0 on PASS, 1 on FAIL.
#
# The strongest possible validation: not "within tolerance" but
# bit-for-bit identical. Holds because:
#   * Each cell write in step() goes to a unique (i, j); OpenMP
#     parallelism is order-independent.
#   * MPI_Sendrecv copies bytes exactly.
#   * PGM normalization uses the global min/max, both deterministic
#     given identical inputs.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${PROJECT_DIR}/build/stencilflow"
MPIRUN="${MPIRUN:-/opt/homebrew/bin/mpirun}"

if [[ ! -x "${BIN}" ]]; then
    echo "validate: ${BIN} not found, did you build?" >&2
    exit 1
fi

ROWS=128
COLS=128
STEPS=200
SAVE_EVERY=100000  # huge so only the final frame is saved

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

run_and_capture() {
    local n_ranks="$1"
    local out_dir="$2"
    mkdir -p "${out_dir}"
    cd "${out_dir}"
    "${MPIRUN}" -n "${n_ranks}" "${BIN}" "${ROWS}" "${COLS}" "${STEPS}" "${SAVE_EVERY}" \
        > "${out_dir}/run.log" 2>&1
    cd - > /dev/null
}

echo "validate: running serial (-n 1) and hybrid (-n 4)"
run_and_capture 1 "${WORK}/serial"
run_and_capture 4 "${WORK}/hybrid"

FINAL_FRAME="frames/frame_$(printf '%05d' ${STEPS}).pgm"
SERIAL_PGM="${WORK}/serial/${FINAL_FRAME}"
HYBRID_PGM="${WORK}/hybrid/${FINAL_FRAME}"

if [[ ! -f "${SERIAL_PGM}" ]]; then
    echo "validate: serial run did not produce ${SERIAL_PGM}" >&2
    cat "${WORK}/serial/run.log" >&2
    exit 1
fi
if [[ ! -f "${HYBRID_PGM}" ]]; then
    echo "validate: hybrid run did not produce ${HYBRID_PGM}" >&2
    cat "${WORK}/hybrid/run.log" >&2
    exit 1
fi

if cmp -s "${SERIAL_PGM}" "${HYBRID_PGM}"; then
    echo "validate: PASS, byte-identical final frames at step ${STEPS}"
    echo "  size:     ${ROWS}x${COLS}"
    echo "  steps:    ${STEPS}"
    echo "  bytes:    $(wc -c < "${SERIAL_PGM}") (each)"
    echo "  serial:   ${SERIAL_PGM}"
    echo "  hybrid:   ${HYBRID_PGM}"
    exit 0
else
    echo "validate: FAIL, PGMs differ"
    cmp -l "${SERIAL_PGM}" "${HYBRID_PGM}" | head -5 >&2
    exit 1
fi
