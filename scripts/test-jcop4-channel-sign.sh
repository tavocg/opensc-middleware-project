#!/usr/bin/env bash
# Synthetic software RSA/PIN/reader only; no actual card/GUI/PIN store.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
read -r -a task_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_libs <<< "$(pkg-config --libs libcrypto)"
# Compile the actual hidden byte serializer for the software fixture only.
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror -ffunction-sections \
    -Dsc_transmit_apdu=jcop4_fixture_unused_transmit -Dsc_format_apdu=jcop4_fixture_unused_format \
    -I"$task_build" -I"$task_root/vendor/opensc/src" \
    -c "$task_root/vendor/opensc/src/libopensc/apdu.c" -o "$task_build/jcop4-sign-wire-fixture.o"
for task_fixture in native bootstrap; do
    task_defines=(-D_GNU_SOURCE -DHAVE_CONFIG_H)
    if [[ $task_fixture == bootstrap ]]; then task_defines+=(-DTEST_REAL_BOOTSTRAP); fi
    "${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${task_defines[@]}" \
        -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_flags[@]}" \
        "$task_root/tests/jcop4-channel-sign.c" "$task_build/jcop4-sign-wire-fixture.o" \
        -Wl,--gc-sections -L"$task_build/src/libopensc/.libs" \
        -Wl,-rpath,"$task_build/src/libopensc/.libs" -lopensc "${task_libs[@]}" \
        -o "$task_build/jcop4-channel-sign-$task_fixture-regression"
    env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE -u OPENSC_DEEPBIND -u LD_PRELOAD -u LD_AUDIT \
        -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_DISCOVERY_TRACE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
        "$task_build/jcop4-channel-sign-$task_fixture-regression" </dev/null
done
