#!/usr/bin/env bash
# Offline callback fixtures only, no PCSC or card entrypoint.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$task_root/build/diagnostics"
read -r -a task_crypto_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_crypto_libs <<< "$(pkg-config --libs libcrypto)"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${task_crypto_flags[@]}" \
    "$task_root/tests/jcop4-response-collector.c" "${task_crypto_libs[@]}" \
    -o "$task_root/build/diagnostics/jcop4-response-collector-regression"
"$task_root/build/diagnostics/jcop4-response-collector-regression" </dev/null
