#!/usr/bin/env bash
# GUI launch only. The user chooses the document and enters the PIN in Java.
set -euo pipefail
umask 077
ulimit -c 0
if (( $# )); then
    printf 'Uso: make firmador/jcop4\n' >&2; exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
task_module="$task_build/src/pkcs11/.libs/opensc-pkcs11.so"
[[ -r "$task_module" && -r "$task_build/src/libopensc/.libs/libopensc.so" ]] || {
    printf 'Falta el módulo local o libOpenSC; ejecutar make build/opensc.\n' >&2; exit 1
}
command -v firmador >/dev/null || { printf 'No se encuentra firmador en PATH.\n' >&2; exit 1; }
mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/jcop4-firmador-api-XXXXXXXX")
task_config="$task_dir/opensc.conf"
task_trace="$task_dir/api.log"
task_report="$task_dir/report.txt"
: > "$task_trace"
cat > "$task_config" <<'CONFIG'
app default {
    debug = 0;
    card_drivers = iasecc;
    card_driver iasecc { chipdoc_profile = cr-rsa2048; }
    reader_driver pcsc {
        enable_pinpad = false;
        enable_pace = false;
        disconnect_action = leave;
        transaction_end_action = leave;
        reconnect_action = leave;
    }
    framework pkcs15 {
        use_file_caching = no;
        use_pin_caching = no;
        try_emulation_first = no;
    }
    pkcs11 { atomic = false; lock_login = false; slots_per_card = 1; }
}
CONFIG
{
    printf 'date_utc=%s\nmode=chipdoc_cr_rsa2048_gui\napi_trace=fixed_numeric_metadata_only\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'project_commit=%s\nopensc_commit=%s\n' \
        "$(git -C "$task_root" rev-parse HEAD 2>/dev/null || true)" \
        "$(git -C "$task_root/vendor/opensc" rev-parse HEAD 2>/dev/null || true)"
} > "$task_report"
printf 'Informe: %s\nMetadatos API: %s\n' "$task_report" "$task_trace"
# Preserve desktop/session and trust-store settings; exclude preload, PCSC
# delegates, OpenSC experiments and Java options that could enable debugging.
task_env=(env -i PATH="$PATH")
for task_name in HOME USER LOGNAME LANG LC_ALL DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS \
    XDG_RUNTIME_DIR XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME XDG_DATA_DIRS XDG_CONFIG_DIRS \
    SSL_CERT_FILE SSL_CERT_DIR NIX_SSL_CERT_FILE TZ; do
    if [[ -v $task_name ]]; then task_env+=("$task_name=${!task_name}"); fi
done
task_status=0
# No helper transport guard, one-reader namespace or timeout in this GUI.
# No shell PIN collection, stdin pipeline, command-line PDF or log redirection.
"${task_env[@]}" LD_LIBRARY_PATH="$task_build/src/pkcs11/.libs:$task_build/src/libopensc/.libs" \
    LIBASEP11="$task_module" OPENSC_CONF="$task_config" OPENSC_DEBUG=0 \
    OPENSC_API_TRACE_FILE="$task_trace" firmador || task_status=$?
printf 'firmador_exit_status=%s\n' "$task_status" >> "$task_report"
printf 'Firmador terminó (estado %s). Revisar el PDF y los metadatos; salir no acredita una firma ni cierre de tarjeta.\n' "$task_status"
exit "$task_status"
