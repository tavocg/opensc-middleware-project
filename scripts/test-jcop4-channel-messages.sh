#!/usr/bin/env bash
# Offline candidate message codec checks: no PC/SC, Idopte, GUI or PIN.
set -euo pipefail
umask 077
if (( $# )); then
    printf 'Uso: bash scripts/test-jcop4-channel-messages.sh\n' >&2
    exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/diagnostics"
task_crypto_cflags=$(pkg-config --cflags libcrypto)
task_crypto_libs=$(pkg-config --libs libcrypto)
read -r -a task_crypto_cflags_array <<< "$task_crypto_cflags"
read -r -a task_crypto_libs_array <<< "$task_crypto_libs"
mkdir -p -- "$task_build"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${task_crypto_cflags_array[@]}" \
    "$task_root/tests/jcop4-channel-messages.c" "${task_crypto_libs_array[@]}" \
    -o "$task_build/jcop4-channel-messages-regression"
"$task_build/jcop4-channel-messages-regression" </dev/null
