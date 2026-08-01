#!/usr/bin/env bash
set -euo pipefail

exe=${1:?usage: run_tcp_pmi_test.sh <test-exe> [nranks] [mode] [endpoint-prefix]}
nranks=${2:-2}
mode=${3:-pmi}
endpoint_prefix=${4:-LCT}

choose_port() {
  python3 - <<'PY'
import socket

try:
    s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
    except OSError:
        pass
    s.bind(("::", 0))
except OSError:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", 0))

print(s.getsockname()[1])
s.close()
PY
}

logdir=$(mktemp -d "${TMPDIR:-/tmp}/lct-tcp-pmi.XXXXXX")
status=0
expect_failure=0
expected_failure_pattern=
reject_torchrun_backend=0
pids=()
port=

cleanup() {
  for pid in "${pids[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
    fi
  done
  if [[ $status -eq 0 ]]; then
    rm -rf "$logdir"
  else
    echo "LCT TCP PMI test logs preserved in $logdir" >&2
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

launch_rank() {
  local rank=$1
  (
    unset LCT_PMI_BACKEND
    if [[ $reject_torchrun_backend -eq 1 ]]; then
      export LCT_PMI_BACKEND=torchrun
    elif [[ "$mode" != "fallback-local" ]]; then
      export LCT_PMI_BACKEND=tcp
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
}

launch_ranks() {
  for rank in $(seq 0 $((nranks - 1))); do
    launch_rank "$rank"
  done
}

wait_for_ranks() {
  status=0
  for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
      status=1
    fi
  done
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
  reject-torchrun)
    mode=pmi
    endpoint_prefix=LCT
    nranks=1
    expect_failure=1
    expected_failure_pattern="Unknown env LCT_PMI_BACKEND"
    reject_torchrun_backend=1
    ;;
  *)
    echo "Unknown mode '$mode'" >&2
    exit 2
    ;;
esac

if [[ $expect_failure -eq 0 && "$mode" != "fallback-local" &&
      $nranks -gt 1 ]]; then
  max_port_attempts=5
  for attempt in $(seq 1 "$max_port_attempts"); do
    port=$(choose_port)
    pids=()
    launch_ranks
    wait_for_ranks

    if [[ $status -eq 0 ]]; then
      break
    fi
    if [[ ! -f "$logdir/rank-0.log" ]] ||
       ! grep -q "Address already in use" "$logdir/rank-0.log"; then
      break
    fi

    if [[ $attempt -lt $max_port_attempts ]]; then
      echo "TCP PMI port $port was claimed before rank 0 started; retrying" >&2
      rm -f "$logdir"/*.log
    fi
  done
else
  port=$(choose_port)
  launch_ranks
  wait_for_ranks
fi

if [[ $expect_failure -eq 1 ]]; then
  if [[ $status -ne 0 ]]; then
    if [[ -n "$expected_failure_pattern" ]] &&
       ! grep -q "$expected_failure_pattern" "$logdir"/*.log; then
      echo "Expected failure did not contain '$expected_failure_pattern'" >&2
      status=1
    else
      status=0
    fi
  else
    echo "Expected $mode configuration to fail, but it succeeded" >&2
    status=1
  fi
fi

exit "$status"
