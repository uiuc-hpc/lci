#!/usr/bin/env bash
set -euo pipefail

lcrun=${1:?usage: run_file_pmi_test.sh <lcrun> <test-exe>}
exe=${2:?usage: run_file_pmi_test.sh <lcrun> <test-exe>}

test_root=$(mktemp -d "${TMPDIR:-/tmp}/lct-file-pmi-test.XXXXXX")
trap 'rm -rf "$test_root"' EXIT
mkdir -p "$test_root/home/.tmp/lct_pmi_file-0" "$test_root/tmp"

# A stale legacy rendezvous must not affect an lcrun invocation.
printf '1' >"$test_root/home/.tmp/lct_pmi_file-0/nranks"
HOME="$test_root/home" TMPDIR="$test_root/tmp" \
  "$lcrun" -n 2 "$exe" 2

# Concurrent launchers must use independent rendezvous directories.
HOME="$test_root/home" TMPDIR="$test_root/tmp" \
  "$lcrun" -n 2 "$exe" 2 &
pid1=$!
HOME="$test_root/home" TMPDIR="$test_root/tmp" \
  "$lcrun" -n 2 "$exe" 2 &
pid2=$!
wait "$pid1"
wait "$pid2"

# lcrun must remove its private rendezvous directories.
if find "$test_root/tmp" -mindepth 1 -print -quit | grep -q .; then
  echo "lcrun left a rendezvous directory behind" >&2
  exit 1
fi

# Run a command, killing it if it is still running after $1 seconds.
run_with_timeout() {
  local seconds=$1
  shift
  "$@" &
  local pid=$!
  # Detached from stdout/stderr, so a sleep left behind cannot hold the test's
  # output open after the command has finished.
  (sleep "$seconds" && kill -9 "$pid") </dev/null >/dev/null 2>&1 &
  local watchdog=$!
  local status=0
  wait "$pid" || status=$?
  kill "$watchdog" 2>/dev/null || true
  wait "$watchdog" 2>/dev/null || true
  return $status
}
clean_env=(env -u LCT_PMI_FILE_NRANKS -u LCT_PMI_FILE_DIR -u SLURM_JOBID
  -u SLURM_STEP_NUM_TASKS -u SLURM_NTASKS)

# A process started directly in a shell inside a Slurm job (a batch script, an
# interactive session) sees the job's SLURM_NTASKS but is not part of an srun
# step: it must not wait for SLURM_NTASKS ranks, so the file backend declines
# and the local one runs it as a single rank.
mkdir -p "$test_root/home-direct"
if ! run_with_timeout 30 "${clean_env[@]}" HOME="$test_root/home-direct" \
    LCT_PMI_BACKEND=file,local SLURM_NTASKS=2 "$exe" 1; then
  echo "a direct start with SLURM_NTASKS=2 did not run as one rank" >&2
  exit 1
fi

# Tasks of an srun step without a PMI service (srun --mpi=none) have
# SLURM_STEP_NUM_TASKS, and the file backend must still join them.
mkdir -p "$test_root/home-step"
pids=()
for _ in 1 2; do
  run_with_timeout 30 "${clean_env[@]}" HOME="$test_root/home-step" \
    LCT_PMI_BACKEND=file SLURM_NTASKS=2 SLURM_STEP_NUM_TASKS=2 "$exe" 2 &
  pids+=($!)
done
for pid in "${pids[@]}"; do
  if ! wait "$pid"; then
    echo "two tasks with SLURM_STEP_NUM_TASKS=2 did not form two ranks" >&2
    exit 1
  fi
done

# A failed child must terminate its peer instead of leaving lcrun blocked.
if HOME="$test_root/home" TMPDIR="$test_root/tmp" "$lcrun" -n 2 \
    python3 -c 'import os, sys, time
try:
    os.mkdir(sys.argv[1] + "/once")
except FileExistsError:
    time.sleep(30)
else:
    sys.exit(7)' "$test_root"; then
  echo "lcrun unexpectedly succeeded after a child failure" >&2
  exit 1
else
  status=$?
fi
if [[ $status -ne 7 ]]; then
  echo "lcrun returned $status instead of the child status 7" >&2
  exit 1
fi
