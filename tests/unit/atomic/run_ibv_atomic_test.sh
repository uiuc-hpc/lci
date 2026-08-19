#!/usr/bin/env bash
# Copyright (c) 2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

set -euo pipefail

if [[ "$#" -eq 0 ]]; then
  echo "Usage: $0 <launcher> [launcher arguments] <test> --require-ibv" >&2
  exit 2
fi

has_active_port=0
for state_file in /sys/class/infiniband/*/ports/*/state; do
  [[ -r "${state_file}" ]] || continue
  if grep -q "ACTIVE" "${state_file}"; then
    has_active_port=1
    break
  fi
done

if [[ "${has_active_port}" -eq 0 ]]; then
  echo "SKIP: no active IBV port is available for the uint64 atomic test" >&2
  exit 77
fi

export LCI_ATTR_BACKEND=ibv
exec "$@"
