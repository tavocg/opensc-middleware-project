#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build=${1:-"$task_root/build/opensc"}
if (( $# > 1 )) || [[ ! -f $task_build/config.h || ! -f $task_build/src/libopensc/.libs/libopensc.so ]]; then
    printf 'Falta OpenSC compilado; ejecutar make build/opensc.\n' >&2
    exit 1
fi
task_build=$(cd -- "$task_build" && pwd)
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections -Wno-unused-parameter \
    -I"$task_build" -I"$task_root/vendor/opensc/src" \
    "$task_root/tools/card-sign-test.c" "$task_root/vendor/opensc/src/tools/util.c" \
    -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -Wl,--gc-sections -lopensc -o "$task_build/card-sign-test"
