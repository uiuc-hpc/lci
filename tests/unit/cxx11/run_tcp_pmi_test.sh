#!/usr/bin/env bash
set -euo pipefail

exe=${1:?usage: run_tcp_pmi_test.sh <test-exe> [nranks] [mode] [endpoint-prefix]}
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
logdir=$(mktemp -d "${TMPDIR:-/tmp}/lct-tcp-pmi.XXXXXX")
status=0
expect_failure=0
expected_failure_pattern=
reject_torchrun_backend=0
pids=()

verify_endpoint_readiness_barriers() {
  local log
  local marker_line
  local pmi_barriers
  for log in "$logdir"/*.log; do
    # Initial OFI address exchange uses one PMI barrier. Endpoint readiness
    # must add the second before g_runtime_init() returns.
    marker_line=$(grep -n -m1 "LCI_ENDPOINT_READINESS_INIT_COMPLETE" "$log" |
      cut -d: -f1 || true)
    if [[ -z "$marker_line" ]]; then
      echo "Missing endpoint-readiness initialization marker in $log" >&2
      return 1
    fi
    pmi_barriers=$(head -n "$marker_line" "$log" |
      grep -c "enter pmi barrier" || true)
    if [[ "$pmi_barriers" -ne 2 ]]; then
      echo "Expected two PMI barriers before endpoint-readiness initialization" \
           "completed in $log, found $pmi_barriers" >&2
      return 1
    fi
  done
}

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

case "$mode" in
  pmi|runtime|endpoint-readiness)
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

for rank in $(seq 0 $((nranks - 1))); do
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
    if [[ "$mode" == "endpoint-readiness" ]]; then
      export LCI_ENABLE_BOOTSTRAP_LCI=1
      export LCI_ATTR_BACKEND=ofi
      # Override inherited provider selection so LCI's hint and libfabric's
      # FI_PROVIDER filter agree.
      export LCI_ATTR_OFI_PROVIDER_NAME=sockets
      export FI_PROVIDER=sockets
      export LCI_ATTR_ALLOC_DEFAULT_DEVICE=1
      export LCI_ATTR_ALLOC_DEFAULT_PACKET_POOL=1
      export LCI_ATTR_ALLOC_DEFAULT_ENDPOINT=1
      export LCI_ATTR_ALLOC_PROGRESS_ENDPOINT=0
      export LCI_ATTR_PACKET_SIZE=256
      export LCI_ATTR_NPACKETS=128
      export LCI_ATTR_NET_MAX_RECVS=64
      export LCI_ATTR_SHM_ENABLE=0
      export LCT_LOG_LEVEL=debug
    else
      export LCI_ENABLE_BOOTSTRAP_LCI=0
    fi
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

if [[ $status -eq 0 && "$mode" == "endpoint-readiness" ]]; then
  if ! verify_endpoint_readiness_barriers; then
    status=1
  fi
fi

exit "$status"
