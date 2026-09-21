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
