#!/usr/bin/env bash
set -euo pipefail
umask 077
if (( $# )); then exit 2; fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
read -r -a task_flags <<< "$(pkg-config --cflags libpcsclite)"
read -r -a task_libs <<< "$(pkg-config --libs libpcsclite)"
mkdir -p -- "$task_root/build/diagnostics"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${task_flags[@]}" \
    "$task_root/tools/jcop4-reader-features.c" "${task_libs[@]}" \
    -o "$task_root/build/diagnostics/jcop4-reader-features"
