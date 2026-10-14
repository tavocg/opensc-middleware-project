#!/usr/bin/env bash
# Local profile/lifecycle only. No reader/card/PIN/GUI or PCSC context.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
read -r -a task_cflags <<< "$(pkg-config --cflags libcrypto)"
read -r -a task_libs <<< "$(pkg-config --libs libcrypto)"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter -Wno-deprecated-declarations \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_cflags[@]}" \
    "$task_root/tests/iasecc-chipdoc-profile.c" \
    -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -Wl,--gc-sections -lopensc "${task_libs[@]}" -o "$task_build/iasecc-chipdoc-profile-regression"
"$task_build/iasecc-chipdoc-profile-regression" </dev/null
