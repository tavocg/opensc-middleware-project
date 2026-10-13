#!/usr/bin/env bash
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$task_root/build/diagnostics"
read -r -a task_pcsc_cflags <<< "$(pkg-config --cflags libpcsclite)"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fPIC -shared \
    "${task_pcsc_cflags[@]}" "$task_root/tools/pcsc-public-observer.c" \
    -ldl -pthread -o "$task_root/build/diagnostics/pcsc-public-observer.so"
