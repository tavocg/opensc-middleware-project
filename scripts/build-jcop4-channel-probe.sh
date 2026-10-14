#!/usr/bin/env bash
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build=${1:-"$task_root/build/opensc"}
if (( $# > 1 )) || [[ ! -f $task_build/config.h || ! -f $task_build/src/libopensc/.libs/libopensc.so ]]; then
    printf 'Falta una compilación de OpenSC; ejecutar make build/opensc.\n' >&2
    exit 1
fi
task_build=$(cd -- "$task_build" && pwd)
task_crypto_cflags=$(pkg-config --cflags libcrypto)
task_crypto_libs=$(pkg-config --libs libcrypto)
read -r -a task_crypto_cflags_array <<< "$task_crypto_cflags"
read -r -a task_crypto_libs_array <<< "$task_crypto_libs"

"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_crypto_cflags_array[@]}" \
    "$task_root/tools/jcop4-channel-probe.c" \
    -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -lopensc "${task_crypto_libs_array[@]}" -o "$task_build/jcop4-channel-probe"
