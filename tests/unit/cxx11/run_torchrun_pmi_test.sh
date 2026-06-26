#!/usr/bin/env bash
set -euo pipefail

exe=${1:?usage: run_torchrun_pmi_test.sh <test-exe> [nranks] [mode] [endpoint-prefix]}
nranks=${2:-2}
mode=${3:-pmi}
endpoint_prefix=${4:-LCT}

port=$(python3 - <<'PY'
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
)
logdir=$(mktemp -d "${TMPDIR:-/tmp}/lct-torchrun-pmi.XXXXXX")
status=0
expect_failure=0
pids=()

cleanup() {
  for pid in "${pids[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
    fi
  done
  if [[ $status -eq 0 ]]; then
    rm -rf "$logdir"
  else
    echo "LCT torchrun PMI test logs preserved in $logdir" >&2
    for f in "$logdir"/*; do
      [[ -f "$f" ]] || continue
      echo "--- $f ---" >&2
      cat "$f" >&2
    done
  fi
}
trap cleanup EXIT

set_endpoint_env() {
  unset LCT_MASTER_ADDR LCT_MASTER_PORT LCI_MASTER_ADDR LCI_MASTER_PORT \
    MASTER_ADDR MASTER_PORT

  case "$endpoint_prefix" in
    LCT)
      export LCT_MASTER_ADDR=127.0.0.1
      export LCT_MASTER_PORT=$port
      ;;
    LCI)
      export LCI_MASTER_ADDR=127.0.0.1
      export LCI_MASTER_PORT=$port
      ;;
    MASTER)
      export MASTER_ADDR=127.0.0.1
      export MASTER_PORT=$port
      ;;
    MIXED)
      # This deliberately does not form a matched pair. The backend must not
      # mix LCT_MASTER_ADDR with MASTER_PORT and accidentally start.
      export LCT_MASTER_ADDR=127.0.0.1
      export MASTER_PORT=$port
      ;;
    NONE)
      ;;
    *)
      echo "Unknown endpoint prefix '$endpoint_prefix'" >&2
      exit 2
      ;;
  esac
}

case "$mode" in
  pmi|runtime)
    ;;
  fallback-local)
    endpoint_prefix=NONE
    nranks=1
    ;;
  mixed-expect-fail)
    mode=pmi
    endpoint_prefix=MIXED
    expect_failure=1
    ;;
  *)
    echo "Unknown mode '$mode'" >&2
    exit 2
    ;;
esac

for rank in $(seq 0 $((nranks - 1))); do
  (
    unset LCT_PMI_BACKEND
    if [[ "$mode" != "fallback-local" ]]; then
      export LCT_PMI_BACKEND=torchrun
    fi
    export RANK=$rank
    export WORLD_SIZE=$nranks
    export LOCAL_RANK=$rank
    export LOCAL_WORLD_SIZE=$nranks
    export LCT_PMI_TCP_TIMEOUT_SEC=10
    export LCI_ENABLE_BOOTSTRAP_LCI=0
    set_endpoint_env
    exec "$exe" "$mode"
  ) >"$logdir/rank-$rank.log" 2>&1 &
  pids+=("$!")
done

for pid in "${pids[@]}"; do
  if ! wait "$pid"; then
    status=1
  fi
done

if [[ $expect_failure -eq 1 ]]; then
  if [[ $status -ne 0 ]]; then
    status=0
  else
    echo "Expected mixed endpoint configuration to fail, but it succeeded" >&2
    status=1
  fi
fi

exit "$status"
