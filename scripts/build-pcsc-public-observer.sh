#!/usr/bin/env bash
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$task_root/build/diagnostics"
read -r -a task_pcsc_cflags <<< "$(pkg-config --cflags libpcsclite)"
task_pcsc_libdir=$(pkg-config --variable=libdir libpcsclite)
task_pcsc_real="$task_pcsc_libdir/libpcsclite_real.so.1"
if [[ ! -f $task_pcsc_real ]]; then
    printf 'Falta libpcsclite_real.so.1; este observador requiere pcsc-lite con delegación.\n' >&2
    exit 1
fi
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fPIC -shared \
    "${task_pcsc_cflags[@]}" "$task_root/tools/pcsc-public-observer.c" \
    -ldl -pthread -Wl,--no-as-needed "$task_pcsc_real" -Wl,--as-needed \
    -Wl,-rpath,"$task_pcsc_libdir" -o "$task_root/build/diagnostics/pcsc-public-observer.so"
