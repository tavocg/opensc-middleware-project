#!/usr/bin/env bash
set -euo pipefail

task_jobs=${1:-2}
if (( $# > 1 )) || [[ ! $task_jobs =~ ^[1-9][0-9]*$ ]]; then
    printf 'Uso: bash scripts/build-opensc.sh [procesos]\n' >&2
    exit 2
fi

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_source="$task_root/vendor/opensc"
task_build="$task_root/build/opensc"
if [[ ! -f "$task_source/configure.ac" ]]; then
    printf 'Falta el submódulo OpenSC; ejecutar git submodule update --init.\n' >&2
    exit 1
fi
if ! pkg-config --exists libcrypto libpcsclite zlib; then
    printf 'Faltan dependencias de compilación; ejecutar nix develop.\n' >&2
    exit 1
fi

mkdir -p -- "$task_build"
(cd -- "$task_source" && ./bootstrap)
task_pcsc_libdir=$(pkg-config --variable=libdir libpcsclite)
cd -- "$task_build"
PCSC_CFLAGS=$(pkg-config --cflags libpcsclite) "$task_source/configure" \
    --prefix="$task_root/build/install" \
    --enable-openssl --enable-zlib \
    --with-pcsc-provider="$task_pcsc_libdir/libpcsclite.so.1" \
    --disable-man --disable-doc --disable-notify --disable-readline \
    --disable-cmocka --disable-openpace --disable-dnie-ui
make -j "$task_jobs"

printf '\nHerramientas compiladas en %s/src/tools; no se instala nada en el sistema.\n' "$task_build"
