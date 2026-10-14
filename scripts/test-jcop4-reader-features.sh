#!/usr/bin/env bash
# Link only mocks, never real PC/SC for tests.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
read -r -a task_flags <<< "$(pkg-config --cflags libpcsclite)"
mkdir -p -- "$task_root/build/diagnostics"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -Wall -Wextra -Werror "${task_flags[@]}" \
    "$task_root/tests/jcop4-reader-features.c" -o "$task_root/build/diagnostics/jcop4-reader-features-regression"
env -u LD_PRELOAD -u LD_AUDIT -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE \
    -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE \
    -u JCOP4_PUBLIC_DISCOVERY_TRACE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    "$task_root/build/diagnostics/jcop4-reader-features-regression" </dev/null
python3 "$task_root/tests/jcop4-reader-features-launcher.py"
