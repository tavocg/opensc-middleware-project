#!/usr/bin/env bash
set -euo pipefail
umask 077

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_module="$task_root/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"

if (( $# != 0 )); then
    printf 'Uso: make firmador/jcop3\n' >&2
    exit 2
fi
if [[ ! -r "$task_module" ]]; then
    printf 'Falta el módulo local; ejecutar make build/opensc.\n' >&2
    exit 1
fi
if ! command -v firmador >/dev/null 2>&1; then
    printf 'No se encuentra firmador en PATH.\n' >&2
    exit 1
fi

mkdir -p -- "$task_root/local"
task_config=$(mktemp "$task_root/local/jcop3-firmador-XXXXXXXX.conf")
cat > "$task_config" <<'CONFIG'
app default {
    debug = 0;
    framework pkcs15 {
        use_file_caching = no;
        use_pin_caching = no;
        try_emulation_first = yes;
    }
}
CONFIG

printf 'Módulo: %s\nConfiguración: %s\n' "$task_module" "$task_config"
exec env OPENSC_DEBUG=0 OPENSC_CONF="$task_config" LIBASEP11="$task_module" firmador
