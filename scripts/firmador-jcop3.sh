#!/usr/bin/env bash
set -euo pipefail
umask 077

task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_module="$task_root/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so"
task_trace_api=no
task_session_state=no
task_pin_status_in_place=no

if (( $# == 1 )) && [[ $1 == --trace-api ]]; then
    task_trace_api=yes
elif (( $# == 1 )) && [[ $1 == --session-state ]]; then
    task_trace_api=yes
    task_session_state=yes
elif (( $# == 1 )) && [[ $1 == --pin-status-in-place ]]; then
    task_trace_api=yes
    task_pin_status_in_place=yes
elif (( $# != 0 )); then
    printf 'Uso: make firmador/jcop3, make firmador/jcop3-diagnose, make firmador/jcop3-session-state o make firmador/jcop3-pin-status-in-place\n' >&2
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
task_env=(env -u OPENSC_API_TRACE_FILE -u OPENSC_JCOP3_SESSION_STATE
    -u OPENSC_JCOP3_POST_LOGIN_STATUS -u OPENSC_JCOP3_POST_LOGIN_SELECT
    -u OPENSC_JCOP3_PIN_STATUS_IN_PLACE
    OPENSC_DEBUG=0 LIBASEP11="$task_module")
if [[ $task_trace_api == yes ]]; then
    task_output=$(mktemp -d "$task_root/local/jcop3-firmador-api-XXXXXXXX")
    task_config="$task_output/opensc.conf"
    task_trace="$task_output/api.log"
    : > "$task_trace"
    {
        printf 'date_utc=%s\nmodule=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$task_module"
        printf 'session_state_experiment=%s\n' "$task_session_state"
        printf 'pin_status_in_place_experiment=%s\n' "$task_pin_status_in_place"
        if [[ $task_session_state == yes ]]; then
            printf 'pin_query_experiment=session_and_token_info\n'
        elif [[ $task_pin_status_in_place == yes ]]; then
            printf 'pin_query_experiment=status_without_selection\n'
        fi
        git -C "$task_root/vendor/opensc" rev-parse HEAD
        git -C "$task_root/vendor/opensc" status --short
    } > "$task_output/report.txt"
    task_env+=(OPENSC_API_TRACE_FILE="$task_trace")
    printf 'Informe API (solo metadatos): %s\n' "$task_trace"
else
    task_config=$(mktemp "$task_root/local/jcop3-firmador-XXXXXXXX.conf")
fi
if [[ $task_pin_status_in_place == yes ]]; then
    task_env+=(OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes)
    printf 'Prueba: consulta física del PIN sin reseleccionar ni releer su política conocida.\n'
    printf 'Los intentos restantes proceden de la respuesta actual; pueden no estar disponibles tras login.\n'
fi
if [[ $task_session_state == yes ]]; then
    task_env+=(OPENSC_JCOP3_SESSION_STATE=yes)
    printf 'Experimento: se omiten consultas físicas de PIN de C_GetSessionInfo y C_GetTokenInfo tras login.\n'
    printf 'Las banderas de intentos del token usan los últimos metadatos públicos disponibles.\n'
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
