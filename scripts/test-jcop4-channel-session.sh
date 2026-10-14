#!/usr/bin/env bash
# Crypto/software peer only; never creates a PC/SC context or card connection.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
mkdir -p -- "$task_build"
task_crypto_cflags=$(pkg-config --cflags libcrypto)
task_crypto_libs=$(pkg-config --libs libcrypto)
read -r -a task_crypto_cflags_array <<< "$task_crypto_cflags"
read -r -a task_crypto_libs_array <<< "$task_crypto_libs"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -Wall -Wextra -Werror "${task_crypto_cflags_array[@]}" \
    "$task_root/tests/jcop4-channel-session.c" "${task_crypto_libs_array[@]}" \
    -o "$task_build/jcop4-channel-session-regression"
"$task_build/jcop4-channel-session-regression" </dev/null
