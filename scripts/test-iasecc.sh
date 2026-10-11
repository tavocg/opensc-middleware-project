#!/usr/bin/env bash
set -euo pipefail

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build=${1:-"$task_root/build/opensc"}
if (( $# > 1 )); then
    printf 'Uso: bash scripts/test-iasecc.sh [directorio de compilación]\n' >&2
    exit 2
fi
if [[ ! -f "$task_build/config.h" || ! -f "$task_build/src/libopensc/.libs/libopensc.so" ]]; then
    printf 'Falta una compilación de OpenSC; ejecutar make build/opensc.\n' >&2
    exit 1
fi
task_build=$(cd -- "$task_build" && pwd)

read -r -a task_crypto_cflags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_crypto_libs <<< "$(pkg-config --libs libcrypto)"
for task_test in iasecc-fci iasecc-pin-path iasecc-sdo; do
    "${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H \
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
        -Wno-unused-function -Wno-unused-parameter -Wno-deprecated-declarations \
        -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_crypto_cflags[@]}" \
        "$task_root/tests/$task_test.c" \
        -L"$task_build/src/libopensc/.libs" \
        -Wl,-rpath,"$task_build/src/libopensc/.libs" -Wl,--gc-sections \
        -lopensc "${task_crypto_libs[@]}" -o "$task_build/$task_test-regression"
    "$task_build/$task_test-regression"
done
