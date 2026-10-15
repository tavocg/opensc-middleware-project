#!/usr/bin/env bash
set -euo pipefail
umask 077
(( $# == 0 )) || exit 2
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
[[ -f "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" && -f "$task_build/src/libopensc/.libs/libopensc.so" ]] || {
    printf 'Falta OpenSC compilado; ejecutar make build/opensc.\n' >&2; exit 1;
}
read -r -a task_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_libs <<< "$(pkg-config --libs libcrypto)"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -Wall -Wextra -Werror \
    "-DAUTO_MODULE_PATH=\"$task_build/src/pkcs11/.libs/opensc-pkcs11.so\"" \
    -I"$task_root/vendor/opensc/src" "${task_flags[@]}" \
    "$task_root/tools/pkcs11-auto-sign.c" -ldl "${task_libs[@]}" \
    -o "$task_build/pkcs11-auto-sign"
