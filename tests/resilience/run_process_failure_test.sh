#!/usr/bin/env bash
# Copyright (c) 2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

set -euo pipefail

exe=${1:?usage: run_process_failure_test.sh <test-exe>}
port=$(python3 - <<'PY'
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
)
workdir=$(mktemp -d "${TMPDIR:-/tmp}/lci-process-failure.XXXXXX")
status=0
pids=()

cleanup() {
  for pid in "${pids[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
    fi
  done
  if [[ $status -eq 0 ]]; then
    rm -rf "$workdir"
  else
    echo "LCI process-failure test logs preserved in $workdir" >&2
    for file in "$workdir"/*.log; do
      [[ -f "$file" ]] || continue
      echo "--- $file ---" >&2
      cat "$file" >&2
    done
  fi
}
trap cleanup EXIT

for rank in 0 1 2; do
  (
    export LCT_PMI_BACKEND=tcp
    export LCT_MASTER_ADDR=127.0.0.1
    export LCT_MASTER_PORT="$port"
    export LCT_PMI_TCP_TIMEOUT_SEC=10
    export RANK="$rank"
    export WORLD_SIZE=3
    export LOCAL_RANK="$rank"
    export LOCAL_WORLD_SIZE=3
    export LCI_ATTR_BACKEND=ofi
    export FI_PROVIDER=tcp
    export LCI_ENABLE_BOOTSTRAP_LCI=0
    export LCI_RESILIENCE_TEST_DIR="$workdir"
    exec "$exe"
  ) >"$workdir/rank-$rank.log" 2>&1 &
  pids+=("$!")
done

deadline=$((SECONDS + 15))
while [[ ! -e "$workdir/ready-0" || ! -e "$workdir/ready-1" ||
         ! -e "$workdir/ready-2" ]]; do
  if ((SECONDS >= deadline)); then
    echo "Timed out waiting for all ranks to initialize" >&2
    status=1
    exit "$status"
  fi
  sleep 0.05
done

touch "$workdir/start"
deadline=$((SECONDS + 15))
while [[ ! -e "$workdir/operation-posted" ]]; do
  if ((SECONDS >= deadline)); then
    echo "Timed out waiting for rank 0 to post the failure operation" >&2
    status=1
    exit "$status"
  fi
  sleep 0.05
done

kill -KILL "${pids[1]}"
if wait "${pids[1]}"; then
  echo "Rank 1 unexpectedly exited before it was killed" >&2
  status=1
  exit "$status"
fi
touch "$workdir/peer-killed"

deadline=$((SECONDS + 20))
while [[ ! -e "$workdir/finished-0" || ! -e "$workdir/finished-2" ]]; do
  if ((SECONDS >= deadline)); then
    echo "Timed out waiting for surviving ranks to finish" >&2
    status=1
    exit "$status"
  fi
  sleep 0.05
done

if ! wait "${pids[0]}"; then
  status=1
fi
if ! wait "${pids[2]}"; then
  status=1
fi
exit "$status"
