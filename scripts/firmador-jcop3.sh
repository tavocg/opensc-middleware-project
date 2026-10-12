#!/usr/bin/env bash
set -euo pipefail
umask 077

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_module="$task_root/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
task_trace_api=no
task_session_state=no

if (( $# == 1 )) && [[ $1 == --trace-api ]]; then
    task_trace_api=yes
elif (( $# == 1 )) && [[ $1 == --session-state ]]; then
    task_trace_api=yes
    task_session_state=yes
elif (( $# != 0 )); then
    printf 'Uso: make firmador/jcop3, make firmador/jcop3-diagnose o make firmador/jcop3-session-state\n' >&2
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
task_env=(env -u OPENSC_API_TRACE_FILE -u OPENSC_JCOP3_SESSION_STATE OPENSC_DEBUG=0 LIBASEP11="$task_module")
if [[ $task_trace_api == yes ]]; then
    task_output=$(mktemp -d "$task_root/local/jcop3-firmador-api-XXXXXXXX")
    task_config="$task_output/opensc.conf"
    task_trace="$task_output/api.log"
    : > "$task_trace"
    {
        printf 'date_utc=%s\nmodule=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$task_module"
        printf 'session_state_experiment=%s\n' "$task_session_state"
        git -C "$task_root/vendor/opensc" rev-parse HEAD
        git -C "$task_root/vendor/opensc" status --short
    } > "$task_output/report.txt"
    task_env+=(OPENSC_API_TRACE_FILE="$task_trace")
    printf 'Informe API (solo metadatos): %s\n' "$task_trace"
else
    task_config=$(mktemp "$task_root/local/jcop3-firmador-XXXXXXXX.conf")
fi
if [[ $task_session_state == yes ]]; then
    task_env+=(OPENSC_JCOP3_SESSION_STATE=yes)
    printf 'Experimento: C_GetSessionInfo utiliza el estado del login, sin consultar el PIN después de autenticar.\n'
fi
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
exec "${task_env[@]}" OPENSC_CONF="$task_config" firmador
