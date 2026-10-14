#!/usr/bin/env bash
# Built PKCS11 module; context/discovery/reader/credentials are software only.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
read -r -a task_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_libs <<< "$(pkg-config --libs libcrypto)"
# Export only discovery/context mocks, never included legacy source symbols.
task_exports=()
for task_symbol in sc_context_create sc_release_context sc_ctx_detect_readers sc_ctx_get_reader_count sc_ctx_get_reader sc_detect_card_presence sc_wait_for_event sc_cancel sc_connect_card sc_disconnect_card; do
    task_exports+=("-Wl,--export-dynamic-symbol=$task_symbol")
done
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror -ffunction-sections \
    -Dsc_transmit_apdu=fixture_unused_transmit -Dsc_format_apdu=fixture_unused_format \
    -I"$task_build" -I"$task_root/vendor/opensc/src" -c "$task_root/vendor/opensc/src/libopensc/apdu.c" \
    -o "$task_build/jcop4-pkcs11-sign-wire-fixture.o"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    -Wno-unused-function -Wno-unused-parameter -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_flags[@]}" \
    "-DJCOP4_LOCAL_MODULE_PATH=\"$task_build/src/pkcs11/.libs/opensc-pkcs11.so\"" \
    "$task_root/tests/jcop4-pkcs11-sign.c" "$task_build/jcop4-pkcs11-sign-wire-fixture.o" \
    -Wl,--gc-sections "${task_exports[@]}" -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" -lopensc -ldl "${task_libs[@]}" -o "$task_build/jcop4-pkcs11-sign-regression"
env -u LD_PRELOAD -u LD_AUDIT -u OPENSC_API_TRACE_FILE \
    LD_LIBRARY_PATH="$task_build/src/pkcs11/.libs:$task_build/src/libopensc/.libs" \
    "$task_build/jcop4-pkcs11-sign-regression" "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" </dev/null
