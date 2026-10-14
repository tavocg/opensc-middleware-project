#!/usr/bin/env bash
# Synthetic input, card/context only. No PCSC, /dev/tty, PIN store or GUI.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
read -r -a task_crypto_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_crypto_libs <<< "$(pkg-config --libs libcrypto)"
for task_fixture in native bootstrap; do
    task_defines=(-D_GNU_SOURCE -DHAVE_CONFIG_H)
    if [[ $task_fixture == bootstrap ]]; then task_defines+=(-DTEST_REAL_BOOTSTRAP); fi
    "${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${task_defines[@]}" \
        -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_crypto_flags[@]}" \
        "$task_root/tests/jcop4-channel-login.c" \
        -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
        -lopensc "${task_crypto_libs[@]}" -o "$task_build/jcop4-channel-login-$task_fixture-regression"
    env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE -u OPENSC_DEEPBIND \
        -u LD_PRELOAD -u LD_AUDIT -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE \
        -u JCOP4_PUBLIC_DISCOVERY_TRACE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
        "$task_build/jcop4-channel-login-$task_fixture-regression" </dev/null
done
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_crypto_flags[@]}" \
    "$task_root/tests/jcop4-pin-input.c" \
    -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -lopensc "${task_crypto_libs[@]}" -o "$task_build/jcop4-pin-input-regression"
python3 "$task_root/tests/jcop4-login-terminal.py" "$task_build/jcop4-pin-input-regression"
