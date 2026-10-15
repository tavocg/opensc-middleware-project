#!/usr/bin/env bash
# Distributed GUI launcher: choose a profile; Java reads the PIN itself.
set -euo pipefail
ulimit -c 0
if (( $# != 1 )) || [[ $1 != jcop3 && $1 != jcop4 ]]; then
    printf 'Uso: firmador-jcop jcop3|jcop4\n' >&2; exit 2
fi
task_profile=$1
task_bundle=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_module="$task_bundle/lib/libjcop3p11.so"
task_config="$task_bundle/config/$task_profile.conf"
[[ -r "$task_module" && -r "$task_config" ]] || {
    printf 'Falta la biblioteca o configuración; extraer el paquete completo.\n' >&2; exit 1;
}
command -v firmador >/dev/null || { printf 'No se encuentra firmador en PATH.\n' >&2; exit 1; }
task_env=(env -i PATH="$PATH")
for task_name in HOME USER LOGNAME LANG LC_ALL DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS \
    XDG_RUNTIME_DIR XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME XDG_DATA_DIRS XDG_CONFIG_DIRS \
    SSL_CERT_FILE SSL_CERT_DIR NIX_SSL_CERT_FILE TZ; do
    if [[ -v $task_name ]]; then task_env+=("$task_name=${!task_name}"); fi
done
if [[ $task_profile == jcop3 ]]; then
    task_env+=(OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes)
fi
exec "${task_env[@]}" LD_LIBRARY_PATH="$task_bundle/lib" LIBASEP11="$task_module" \
    OPENSC_CONF="$task_config" OPENSC_DEBUG=0 firmador
