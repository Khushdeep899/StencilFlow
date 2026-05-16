#!/usr/bin/env bash
# Run strong and weak scaling benchmarks for StencilFlow.
#
# Strong scaling: fixed problem size, vary MPI rank count.
#   Plot speedup vs ranks. Ideal is a straight line.
#
# Weak scaling: problem size grows with rank count so each rank
#   has the same amount of work. Plot efficiency = T1 / Tn.
#   Ideal is a flat line at 1.0.
#
# Output: benchmarks/results.csv. Plot with benchmarks/plot.py.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${PROJECT_DIR}/build/stencilflow"
MPIRUN="${MPIRUN:-/opt/homebrew/bin/mpirun}"
OUT="${SCRIPT_DIR}/results.csv"

if [[ ! -x "${BIN}" ]]; then
    echo "run: ${BIN} not found, please build first" >&2
    exit 1
fi

# Disable PGM frame output so timing isolates the solver loop.
SAVE_EVERY=10000000

# Parse the "elapsed: 0.123s, ..." line from a run.
elapsed_from_log() {
    grep -E '^elapsed:' "$1" | awk '{print $2}' | tr -d 's,'
}

mkdir -p "${SCRIPT_DIR}/logs"
echo "mode,ranks,threads,rows,cols,steps,elapsed_s" > "${OUT}"

# Strong scaling: fixed problem big enough that MPI startup overhead
# does not dominate the time at high rank counts.
STRONG_ROWS=1024
STRONG_COLS=1024
STRONG_STEPS=1000
for ranks in 1 2 4 8; do
    log="${SCRIPT_DIR}/logs/strong_${ranks}.log"
    echo "strong: ranks=${ranks}"
    OMP_NUM_THREADS=1 "${MPIRUN}" -n "${ranks}" "${BIN}" \
        "${STRONG_ROWS}" "${STRONG_COLS}" "${STRONG_STEPS}" "${SAVE_EVERY}" \
        > "${log}" 2>&1
    t="$(elapsed_from_log "${log}")"
    echo "strong,${ranks},1,${STRONG_ROWS},${STRONG_COLS},${STRONG_STEPS},${t}" >> "${OUT}"
done

# Weak scaling: 512 rows per rank, 1024 cols, 1000 steps. Bigger
# per-rank work so per-step time dominates MPI overhead.
WEAK_ROWS_PER_RANK=512
WEAK_COLS=1024
WEAK_STEPS=1000
for ranks in 1 2 4 8; do
    rows=$((WEAK_ROWS_PER_RANK * ranks))
    log="${SCRIPT_DIR}/logs/weak_${ranks}.log"
    echo "weak: ranks=${ranks}, rows=${rows}"
    OMP_NUM_THREADS=1 "${MPIRUN}" -n "${ranks}" "${BIN}" \
        "${rows}" "${WEAK_COLS}" "${WEAK_STEPS}" "${SAVE_EVERY}" \
        > "${log}" 2>&1
    t="$(elapsed_from_log "${log}")"
    echo "weak,${ranks},1,${rows},${WEAK_COLS},${WEAK_STEPS},${t}" >> "${OUT}"
done

echo
echo "wrote ${OUT}"
echo
column -s, -t < "${OUT}"
