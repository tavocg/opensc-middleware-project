#!/usr/bin/env bash
# First inventory of an unknown profile: no card driver initialization or APDU.
set -euo pipefail
umask 077

task_reader=${1:-${READER:-0}}
if (( $# > 1 )) || [[ ! $task_reader =~ ^[0-9]+$ ]]; then
    printf 'Uso: bash scripts/probe-jcop4.sh [índice numérico de lector]\n' >&2
    exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tool="$task_root/build/opensc/src/tools/opensc-tool"
if [[ ! -x $task_tool ]]; then
    printf 'Falta OpenSC compilado; ejecutar nix develop y make build/opensc.\n' >&2
    exit 1
fi

mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/jcop4-probe-XXXXXXXX")
task_report="$task_dir/report.txt"
task_config="$task_dir/opensc.conf"
cat > "$task_config" <<'CONFIG'
app default {
    debug = 0;
    framework pkcs15 {
        use_file_caching = no;
        use_pin_caching = no;
    }
}
CONFIG
{
    printf 'date_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'mode=reader_and_atr_only\ncard_label=jcop4_user_supplied\n'
    printf 'reader_index=%s\nopensc_tool=%s\nopensc_conf=%s\n' \
        "$task_reader" "$task_tool" "$task_config"
    if command -v git >/dev/null 2>&1; then
        printf '\nproject_commit:\n'
        git -C "$task_root" rev-parse HEAD || true
        git -C "$task_root" status --short || true
        printf '\nopensc_commit:\n'
        git -C "$task_root/vendor/opensc" rev-parse HEAD || true
        git -C "$task_root/vendor/opensc" status --short || true
    fi
} > "$task_report" 2>&1
printf 'Informe: %s\n' "$task_report"

task_record() {
    local task_status=0
    printf '\n$ opensc-tool' >> "$task_report"
    printf ' %q' "$@" >> "$task_report"
    printf '\n' >> "$task_report"
    env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE \
        -u OPENSC_JCOP3_SESSION_STATE -u OPENSC_JCOP3_PIN_STATUS_IN_PLACE \
        -u OPENSC_JCOP3_POST_LOGIN_STATUS -u OPENSC_JCOP3_POST_LOGIN_SELECT \
        OPENSC_DEBUG=0 OPENSC_CONF="$task_config" \
        "$task_tool" "$@" </dev/null >> "$task_report" 2>&1 || task_status=$?
    printf 'exit_status=%s\n' "$task_status" >> "$task_report"
    if (( task_status != 0 )); then
        printf 'Consulta fallida; revisar el informe antes de continuar.\n' >&2
        exit "$task_status"
    fi
}

task_record --info
# Do not add --verbose: list_readers would initialize card drivers.
task_record --list-readers
# ATR is printed before sc_connect_card() and application identification.
task_record --reader "$task_reader" --atr
printf 'Consulta de lector y ATR terminada. Revisar el informe antes de consultar la aplicación.\n'
