#!/usr/bin/env bash
# Software peer and real OpenSC APDU dispatcher only; no PCSC/card/PIN/GUI.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
read -r -a task_flags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_libs <<< "$(pkg-config --libs libcrypto)"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror -ffunction-sections \
    -Dsc_transmit_apdu=fixture_unused_transmit -Dsc_format_apdu=fixture_unused_format \
    -I"$task_build" -I"$task_root/vendor/opensc/src" -c "$task_root/vendor/opensc/src/libopensc/apdu.c" \
    -o "$task_build/iasecc-chipdoc-sign-wire-fixture.o"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    -Wno-unused-function -Wno-unused-parameter -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_flags[@]}" \
    "$task_root/tests/iasecc-chipdoc-sign.c" "$task_build/iasecc-chipdoc-sign-wire-fixture.o" \
    -Wl,--gc-sections -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -lopensc "${task_libs[@]}" -o "$task_build/iasecc-chipdoc-sign-regression"
"$task_build/iasecc-chipdoc-sign-regression" </dev/null
