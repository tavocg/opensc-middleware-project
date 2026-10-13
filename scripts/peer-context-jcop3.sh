#!/usr/bin/env bash
# A separate, unauthenticated client selects the already observed IAS root.
# Controlled context test: before PIN, or after PIN only when explicitly
# testing interference/lock_login. A login transaction can block this client;
# let firmador finish and close it before waiting for this client to finish.
set -euo pipefail
umask 077
task_reader=${READER:-0}
if (( $# != 0 )) || [[ ! $task_reader =~ ^[0-9]+$ ]]; then
    printf 'Uso: make inspect/jcop3-peer-context READER=0\n' >&2
    exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tool="$task_root/build/opensc/src/tools/opensc-tool"
if [[ ! -x $task_tool ]]; then
    printf 'Falta opensc-tool compilado; ejecutar make build/opensc.\n' >&2
    exit 1
fi
mkdir -p -- "$task_root/local"
task_report=$(mktemp "$task_root/local/jcop3-peer-context-XXXXXXXX.txt")
task_config="$task_report.opensc.conf"
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
    printf 'date_utc=%s\nmode=select_root_no_login\nreader_index=%s\n' \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$task_reader"
    printf 'opensc_tool=%s\nselect_aid=F0496173456363526F6F74\n' "$task_tool"
    git -C "$task_root/vendor/opensc" rev-parse HEAD
    git -C "$task_root/vendor/opensc" status --short
} > "$task_report"
printf 'Informe: %s\n' "$task_report"
task_status=0
env -u OPENSC_API_TRACE_FILE -u OPENSC_JCOP3_SESSION_STATE \
    -u OPENSC_JCOP3_PIN_STATUS_IN_PLACE -u OPENSC_JCOP3_POST_LOGIN_STATUS \
    -u OPENSC_JCOP3_POST_LOGIN_SELECT \
    OPENSC_DEBUG=0 OPENSC_CONF="$task_config" \
    "$task_tool" --reader "$task_reader" --card-driver default \
    --send-apdu 00A404040BF0496173456363526F6F7400 \
    </dev/null >> "$task_report" 2>&1 || task_status=$?
printf 'exit_status=%s\n' "$task_status" >> "$task_report"
if (( task_status != 0 )); then
    printf 'El cliente de consulta falló; detener la prueba y revisar el informe.\n' >&2
    exit "$task_status"
fi
# opensc-tool may return zero even if the card rejects the APDU.
task_select_ok=no
while IFS= read -r task_line; do
    if [[ $task_line == 'Received (SW1=0x90, SW2=0x00)'* ]]; then
        task_select_ok=yes
    fi
done < "$task_report"
if [[ $task_select_ok != yes ]]; then
    printf 'No se confirmó SELECT raíz con 9000; detener la prueba.\n' >&2
    exit 1
fi
printf 'SELECT raíz IAS: 9000. Cliente terminado, sin login ni firma.\n'
