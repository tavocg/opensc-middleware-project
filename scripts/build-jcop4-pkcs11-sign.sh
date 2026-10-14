#!/usr/bin/env bash
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
[[ -f "$task_build/config.h" && -f "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" ]] || { printf 'Falta OpenSC compilado; ejecutar make build/opensc.\n' >&2; exit 1; }
read -r -a task_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_libs <<< "$(pkg-config --libs libcrypto)"
task_exports=()
for task_symbol in sc_context_create sc_release_context sc_ctx_detect_readers sc_ctx_get_reader_count sc_ctx_get_reader sc_connect_card sc_disconnect_card; do
    task_exports+=("-Wl,--export-dynamic-symbol=$task_symbol")
done
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    "-DJCOP4_LOCAL_MODULE_PATH=\"$task_build/src/pkcs11/.libs/opensc-pkcs11.so\"" \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_flags[@]}" \
    "$task_root/tools/jcop4-pkcs11-sign.c" "${task_exports[@]}" \
    -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -lopensc -ldl "${task_libs[@]}" -o "$task_build/jcop4-pkcs11-sign"
