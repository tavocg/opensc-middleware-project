#!/usr/bin/env bash
# Inventory and public application discovery, without login or signing.
set -euo pipefail
umask 077

task_reader=${1:-${READER:-0}}
task_mode=${2:---probe}
if (( $# > 2 )) || [[ ! $task_reader =~ ^[0-9]+$ ]] ||
        [[ $task_mode != --probe && $task_mode != --aids && $task_mode != --files ]]; then
    printf 'Uso: bash scripts/probe-jcop4.sh [índice numérico de lector] [--probe|--aids|--files]\n' >&2
    exit 2
fi
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tool="$task_root/build/opensc/src/tools/opensc-tool"
if [[ ! -x $task_tool ]]; then
    printf 'Falta OpenSC compilado; ejecutar nix develop y make build/opensc.\n' >&2
    exit 1
fi

mkdir -p -- "$task_root/local"
if [[ $task_mode == --aids ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-aids-XXXXXXXX")
elif [[ $task_mode == --files ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-files-XXXXXXXX")
else
    task_dir=$(mktemp -d "$task_root/local/jcop4-probe-XXXXXXXX")
fi
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
    printf 'mode=%s\ncard_label=jcop4_user_supplied\n' "$task_mode"
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
task_query_index=0

task_record_tool() {
    local task_binary=$1 task_status=0
    shift
    task_query_index=$((task_query_index + 1))
    task_last_output="$task_dir/query-$task_query_index.txt"
    printf '\n$ %q' "${task_binary##*/}" >> "$task_report"
    printf ' %q' "$@" >> "$task_report"
    printf '\n' >> "$task_report"
    env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE \
        -u OPENSC_JCOP3_SESSION_STATE -u OPENSC_JCOP3_PIN_STATUS_IN_PLACE \
        -u OPENSC_JCOP3_POST_LOGIN_STATUS -u OPENSC_JCOP3_POST_LOGIN_SELECT \
        OPENSC_DEBUG=0 OPENSC_CONF="$task_config" \
        "$task_binary" "$@" </dev/null > "$task_last_output" 2>&1 || task_status=$?
    cat "$task_last_output" >> "$task_report"
    printf 'exit_status=%s\n' "$task_status" >> "$task_report"
    if (( task_status != 0 )); then
        printf 'Consulta fallida; revisar el informe antes de continuar.\n' >&2
        exit "$task_status"
    fi
}

task_record() { task_record_tool "$task_tool" "$@"; }

task_record --info
# Do not add --verbose: list_readers would initialize card drivers.
task_record --list-readers
# ATR is printed before sc_connect_card() and application identification.
task_record --reader "$task_reader" --atr
if [[ $task_mode == --probe ]]; then
    printf 'Consulta de lector y ATR terminada. Revisar el informe antes de consultar la aplicación.\n'
    exit 0
fi

# Only the card observed in jcop4-probe-9XynOJDU; do not select on another ATR.
task_expected_atr=3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a
task_atr_ok=no
while IFS= read -r task_line; do
    if [[ ${task_line,,} == "$task_expected_atr" ]]; then task_atr_ok=yes; fi
done < "$task_last_output"
if [[ $task_atr_ok != yes ]]; then
    printf 'ATR distinto o ausente; no se consultaron aplicaciones ni archivos. Revisar el informe.\n' >&2
    exit 1
fi

if [[ $task_mode == --files ]]; then
    # Compile only this discovery helper against the existing OpenSC library.
    task_build_status=0
    bash "$task_root/scripts/build-card-public-info.sh" >> "$task_report" 2>&1 || task_build_status=$?
    printf 'helper_build_status=%s\n' "$task_build_status" >> "$task_report"
    if (( task_build_status != 0 )); then
        printf 'Falló la compilación de la herramienta pública; revisar el informe.\n' >&2
        exit "$task_build_status"
    fi
    task_record_tool "$task_root/build/opensc/card-public-info" "$task_reader" "$task_expected_atr"
    printf 'Consulta de EF.ATR/EF.DIR terminada; revisar resultados y aplicaciones del informe.\n'
    exit 0
fi

task_selected_count=0
# A bounded list from the observed JCOP3 and OpenSC's Slovenian-eID wiki.
# Each independent SELECT names its own application; no automatic driver probes.
for task_entry in \
    IAS_root:F0496173456363526F6F74 \
    ChipDoc_Generic:E828BD080FD25047656E65726963 \
    NXP1:E828BD080F014E585031 \
    NXP0:E828BD080F014E585030; do
    task_label=${task_entry%%:*}
    task_aid=${task_entry#*:}
    printf -v task_lc '%02X' "$(( ${#task_aid} / 2 ))"
    printf '\napplication_label=%s\naid=%s\n' "$task_label" "$task_aid" >> "$task_report"
    task_record --reader "$task_reader" --card-driver default \
        --send-apdu "00A40404${task_lc}${task_aid}00"
    # opensc-tool returns zero for card rejection: check the actual SW separately.
    task_sw= task_sw_count=0
    task_sw_regex='^Received \(SW1=0x([[:xdigit:]]{2}), SW2=0x([[:xdigit:]]{2})\):?$'
    while IFS= read -r task_line; do
        if [[ $task_line =~ $task_sw_regex ]]; then
            task_sw=${BASH_REMATCH[1]}${BASH_REMATCH[2]}
            task_sw=${task_sw^^}
            task_sw_count=$((task_sw_count + 1))
        fi
    done < "$task_last_output"
    printf 'response_count=%s\nselect_sw=%s\n' "$task_sw_count" "${task_sw:-missing}" >> "$task_report"
    if (( task_sw_count != 1 )); then
        printf 'No se confirmó una respuesta SELECT única; detenerse y revisar el informe.\n' >&2
        exit 1
    fi
    case $task_sw in
        9000)
            printf 'select_result=selected\n' >> "$task_report"
            task_selected_count=$((task_selected_count + 1)) ;;
        6A82) printf 'select_result=not_found_in_this_query\n' >> "$task_report" ;;
        *)
            printf 'select_result=unexpected_status\n' >> "$task_report"
            printf 'SELECT devolvió %s; detenerse sin autenticación ni reintentos.\n' "$task_sw" >&2
            exit 1 ;;
    esac
done
printf '\nselected_count=%s\n' "$task_selected_count" >> "$task_report"
printf 'Consulta terminada: %s de 4 AID seleccionados; revisar respuestas antes de leer objetos.\n' "$task_selected_count"
