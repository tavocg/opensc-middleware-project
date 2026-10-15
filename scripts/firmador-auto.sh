#!/usr/bin/env bash
# GUI launch only. The user chooses the document and enters the PIN in Java.
set -euo pipefail
umask 077
ulimit -c 0
if (( $# )); then
    printf 'Uso: make firmador/auto\n' >&2; exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
task_module="$task_build/src/pkcs11/.libs/opensc-pkcs11.so"
[[ -r "$task_module" && -r "$task_build/src/libopensc/.libs/libopensc.so" ]] || {
    printf 'Falta el módulo local o libOpenSC; ejecutar make build/opensc.\n' >&2; exit 1
}
command -v firmador >/dev/null || { printf 'No se encuentra firmador en PATH.\n' >&2; exit 1; }
mkdir -p -- "$task_root/local"
task_dir=$(mktemp -d "$task_root/local/auto-firmador-api-XXXXXXXX")
task_trace="$task_dir/api.log"
task_report="$task_dir/report.txt"
: > "$task_trace"
{
    printf 'date_utc=%s\nmode=autodetection_gui\nconfiguration=none_dev_null\ndetection=normal_driver_dispatch\napi_trace=fixed_numeric_metadata_only\njcop3_pin_status=in_place_physical_query\njcop3_verify=fresh_verify\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
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
# The existing library gate limits this correction to Amos/Generic PKI CHV1:
# physical status without reselection, and a fresh VERIFY in its application.
# JCOP4 keeps its native channel/session path. Do not inherit the caller flag.
# No helper transport guard, one-reader namespace or timeout in this GUI.
# No shell PIN collection, stdin pipeline, command-line PDF or log redirection.
"${task_env[@]}" LD_LIBRARY_PATH="$task_build/src/pkcs11/.libs:$task_build/src/libopensc/.libs" \
    LIBASEP11="$task_module" OPENSC_CONF=/dev/null OPENSC_DEBUG=0 \
    OPENSC_API_TRACE_FILE="$task_trace" OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes \
    firmador || task_status=$?
printf 'firmador_exit_status=%s\n' "$task_status" >> "$task_report"
printf 'Firmador terminó (estado %s). Revisar el PDF y los metadatos; salir no acredita una firma ni cierre de tarjeta.\n' "$task_status"
exit "$task_status"
