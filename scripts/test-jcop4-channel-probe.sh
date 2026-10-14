#!/usr/bin/env bash
# Mock OpenSC and software card only; no actual PC/SC connection.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
task_crypto_cflags=$(pkg-config --cflags libcrypto)
task_crypto_libs=$(pkg-config --libs libcrypto)
read -r -a task_crypto_cflags_array <<< "$task_crypto_cflags"
read -r -a task_crypto_libs_array <<< "$task_crypto_libs"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_crypto_cflags_array[@]}" \
    "$task_root/tests/jcop4-channel-probe.c" \
    -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -lopensc "${task_crypto_libs_array[@]}" -o "$task_build/jcop4-channel-probe-regression"
env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE -u OPENSC_DEEPBIND \
    -u LD_PRELOAD -u LD_AUDIT -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE \
    -u JCOP4_PUBLIC_DISCOVERY_TRACE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    "$task_build/jcop4-channel-probe-regression" </dev/null
bash "$task_root/tests/jcop4-channel-launcher.sh"
