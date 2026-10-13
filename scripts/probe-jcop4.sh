#!/usr/bin/env bash
# Inventory and public application discovery, without login or signing.
set -euo pipefail
umask 077

task_reader=${1:-${READER:-0}}
task_mode=${2:---probe}
if (( $# > 2 )) || [[ ! $task_reader =~ ^[0-9]+$ ]] ||
        [[ $task_mode != --probe && $task_mode != --aids && $task_mode != --files && $task_mode != --pkcs15 && $task_mode != --sign-setup && $task_mode != --pin-status && $task_mode != --security-info && $task_mode != --pin-policy && $task_mode != --odf-map && $task_mode != --file4141 && $task_mode != --idopte && $task_mode != --idopte-discovery && $task_mode != --idopte-spy && $task_mode != --idopte-compare ]]; then
    printf 'Uso: bash scripts/probe-jcop4.sh [índice numérico de lector] [--probe|--aids|--files|--pkcs15|--sign-setup|--pin-status|--security-info|--pin-policy|--odf-map|--file4141|--idopte|--idopte-discovery|--idopte-spy|--idopte-compare]\n' >&2
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
elif [[ $task_mode == --pkcs15 ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-pkcs15-XXXXXXXX")
elif [[ $task_mode == --sign-setup ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-sign-setup-XXXXXXXX")
elif [[ $task_mode == --pin-status ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-pin-status-XXXXXXXX")
elif [[ $task_mode == --pin-policy ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-pin-policy-XXXXXXXX")
elif [[ $task_mode == --odf-map ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-odf-map-XXXXXXXX")
elif [[ $task_mode == --file4141 ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-file4141-XXXXXXXX")
elif [[ $task_mode == --idopte ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-idopte-XXXXXXXX")
elif [[ $task_mode == --idopte-spy ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-idopte-spy-XXXXXXXX")
elif [[ $task_mode == --idopte-compare ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-idopte-compare-XXXXXXXX")
elif [[ $task_mode == --idopte-discovery ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-idopte-discovery-XXXXXXXX")
elif [[ $task_mode == --security-info ]]; then
    task_dir=$(mktemp -d "$task_root/local/jcop4-security-info-XXXXXXXX")
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
task_command_env=()
task_allow_diagnostic_failure=no

task_record_tool() {
    local task_binary=$1 task_status=0
    shift
    task_query_index=$((task_query_index + 1))
    task_last_output="$task_dir/query-$task_query_index.txt"
    printf '\n$ %q' "${task_binary##*/}" >> "$task_report"
    printf ' %q' "$@" >> "$task_report"
    printf '\n' >> "$task_report"
    env -u OPENSC_API_TRACE_FILE -u OPENSC_DEBUG_FILE -u OPENSC_DEEPBIND -u LD_PRELOAD -u JCOP4_PUBLIC_DISCOVERY_TRACE \
        -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE -u JCOP4_DIAGNOSTIC_READ4141 \
        -u OPENSC_JCOP3_SESSION_STATE -u OPENSC_JCOP3_PIN_STATUS_IN_PLACE \
        -u OPENSC_JCOP3_POST_LOGIN_STATUS -u OPENSC_JCOP3_POST_LOGIN_SELECT \
        OPENSC_DEBUG=0 OPENSC_CONF="$task_config" "${task_command_env[@]}" \
        "$task_binary" "$@" </dev/null > "$task_last_output" 2>&1 || task_status=$?
    cat "$task_last_output" >> "$task_report"
    printf 'exit_status=%s\n' "$task_status" >> "$task_report"
    if (( task_status != 0 )); then
        if [[ $task_allow_diagnostic_failure == yes ]]; then
            return "$task_status"
        fi
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

if [[ $task_mode == --idopte || $task_mode == --idopte-discovery || $task_mode == --idopte-spy || $task_mode == --idopte-compare ]]; then
    task_pkcs11_tool="$task_root/build/opensc/src/tools/pkcs11-tool"
    task_idopte_module=/run/current-system/sw/lib/SCMiddleware/libidop11.so
    if [[ ! -x $task_pkcs11_tool || ! -f $task_idopte_module ]]; then
        printf 'Falta pkcs11-tool compilado o el módulo Idopte instalado; revisar las rutas del informe.\n' >&2
        printf 'pkcs11_tool=%s\nidopte_module=%s\n' "$task_pkcs11_tool" "$task_idopte_module" >> "$task_report"
        exit 1
    fi
    if [[ $task_mode == --idopte-discovery || $task_mode == --idopte-spy || $task_mode == --idopte-compare ]]; then
        if [[ $task_mode == --idopte-spy ]] && { ! command -v pcsc-spy >/dev/null 2>&1 || ! command -v python3 >/dev/null 2>&1; }; then
            printf 'Falta pcsc-spy o Python 3; ejecutar desde nix develop.\n' >&2
            exit 1
        fi
        task_build_status=0
        bash "$task_root/scripts/build-pcsc-public-observer.sh" >> "$task_report" 2>&1 || task_build_status=$?
        printf 'observer_build_status=%s\n' "$task_build_status" >> "$task_report"
        if (( task_build_status != 0 )); then
            printf 'Falló la compilación del observador público; revisar el informe.\n' >&2
            exit "$task_build_status"
        fi
        task_trace="$task_dir/public-headers.txt"
        task_command_env=("LIBPCSCLITE_DELEGATE=$task_root/build/diagnostics/pcsc-public-observer.so" "JCOP4_PUBLIC_DISCOVERY_TRACE=$task_trace")
        if [[ $task_mode == --idopte-compare ]]; then
            task_command_env+=("JCOP4_DIAGNOSTIC_READ4141=1")
            printf 'comparison_scope=read4141_max400_no_recording_no_authentication\n' >> "$task_report"
        fi
        if [[ $task_mode == --idopte-spy ]]; then
            task_spy_raw="$task_dir/select4141-spy.raw"
            task_command_env+=("JCOP4_PUBLIC_SELECT_SPY_TRACE=$task_spy_raw")
        fi
        task_allow_diagnostic_failure=yes
        task_status=0
        task_record_tool "$task_pkcs11_tool" --module "$task_idopte_module" --show-info --list-slots || task_status=$?
        task_events_observed=no
        task_lifecycle_observed=no
        if [[ -f $task_trace ]]; then
            while IFS= read -r task_line; do
                case $task_line in
                    public_header=*|public_control_code=*|blocked_header=*|blocked=*) task_events_observed=yes ;;
                    pcsc_call=*) task_lifecycle_observed=yes ;;
                esac
            done < "$task_trace"
        fi
        if [[ -f $task_trace ]]; then
            printf '\nobserver_headers:\n' >> "$task_report"
            cat "$task_trace" >> "$task_report"
        fi
        printf 'module_exit_status=%s\n' "$task_status" >> "$task_report"
        if [[ $task_mode == --idopte-spy ]]; then
            if [[ ! -s $task_spy_raw ]]; then
                printf 'spy_result=no_select4141_captured\n' >> "$task_report"
                printf 'No se capturó SELECT 4141; revisar las cabeceras antes de repetir.\n' >&2
                exit 1
            fi
            task_spy_status=0
            python3 "$(command -v pcsc-spy)" --nocolor "$task_spy_raw" </dev/null > "$task_dir/select4141-spy.txt" 2>&1 || task_spy_status=$?
            task_spy_decoded=no
            while IFS= read -r task_line; do
                [[ $task_line != SCardTransmit* ]] || task_spy_decoded=yes
                if [[ $task_line == *Traceback* || $task_line == *Exception* ]]; then task_spy_status=1; fi
            done < "$task_dir/select4141-spy.txt"
            if [[ $task_spy_decoded != yes && $task_spy_status == 0 ]]; then task_spy_status=1; fi
            printf 'spy_decode_status=%s\n' "$task_spy_status" >> "$task_report"
            if (( task_spy_status != 0 )); then
                printf 'Falló la lectura de la captura con pcsc-spy; revisar los archivos privados.\n' >&2
                exit "$task_spy_status"
            fi
            printf 'spy_result=select4141_control_data_only\n' >> "$task_report"
            printf 'Captura privada SELECT 4141: %s\n' "$task_dir/select4141-spy.txt"
        fi
        if [[ $task_events_observed != yes && $task_lifecycle_observed == yes ]]; then
            printf 'observer_result=pcsc_lifecycle_only\n' >> "$task_report"
            printf 'Se observaron conexiones PC/SC, pero no APDU ni controles; revisar sus resultados en el informe.\n' >&2
            exit 1
        elif [[ $task_events_observed != yes ]]; then
            printf 'observer_result=no_pcsc_event_observed\n' >> "$task_report"
            printf 'No se observaron llamadas PC/SC; revisar el informe.\n' >&2
            exit 1
        fi
        printf 'observer_result=events_recorded\n' >> "$task_report"
        printf 'Descubrimiento público observado. El guard puede bloquear comandos y provocar error de Idopte; revisar el informe antes de continuar.\n'
        exit "$task_status"
    fi
    # Module/slot/token information only: no session, C_Login or object access.
    task_record_tool "$task_pkcs11_tool" --module "$task_idopte_module" --show-info --list-slots
    printf 'Consulta pública de Idopte terminada, sin login ni PIN. Revisar los flags de los tokens en el informe.\n'
    exit 0
fi

if [[ $task_mode == --files || $task_mode == --pkcs15 || $task_mode == --sign-setup || $task_mode == --pin-status || $task_mode == --security-info || $task_mode == --pin-policy || $task_mode == --odf-map || $task_mode == --file4141 ]]; then
    # Compile only this discovery helper against the existing OpenSC library.
    task_build_status=0
    bash "$task_root/scripts/build-card-public-info.sh" >> "$task_report" 2>&1 || task_build_status=$?
    printf 'helper_build_status=%s\n' "$task_build_status" >> "$task_report"
    if (( task_build_status != 0 )); then
        printf 'Falló la compilación de la herramienta pública; revisar el informe.\n' >&2
        exit "$task_build_status"
    fi
    if [[ $task_mode == --pkcs15 || $task_mode == --sign-setup || $task_mode == --pin-status || $task_mode == --security-info || $task_mode == --pin-policy || $task_mode == --odf-map || $task_mode == --file4141 ]]; then
        # Advertised in EF.DIR in jcop4-files-DORk47gE, not inferred from ATR.
        task_aid=E828BD080FA00000036364796E6964
        printf 'pkcs15_aid=%s\n' "$task_aid" >> "$task_report"
        task_record_tool "$task_root/build/opensc/card-public-info" "$task_reader" "$task_expected_atr" "$task_mode" "$task_aid"
        if [[ $task_mode == --file4141 ]]; then
            printf 'Consulta SELECT/metadatos de 4141 terminada, sin lectura de contenido ni PIN. Revisar el informe.\n'
        elif [[ $task_mode == --odf-map ]]; then
            printf 'Consulta del directorio público 5031 terminada; no se siguieron referencias ni se introdujo PIN. Revisar el informe.\n'
        elif [[ $task_mode == --pin-policy ]]; then
            printf 'Consulta de política pública del PIN terminada, sin PIN ni autenticación. Revisar el informe.\n'
        elif [[ $task_mode == --security-info ]]; then
            printf 'Consulta de metadatos de seguridad terminada, sin PIN ni autenticación. Revisar el informe.\n'
        elif [[ $task_mode == --pin-status ]]; then
            printf 'Consulta de estado mediante VERIFY vacío terminada; no se introdujo ningún PIN. Revisar el informe.\n'
        elif [[ $task_mode == --sign-setup ]]; then
            printf 'Selección temporal MSE aceptada; no se introdujo PIN ni se produjo una firma. Revisar el informe.\n'
        else
            printf 'Consulta PKCS#15 pública terminada; revisar certificados y metadatos del informe.\n'
        fi
    else
        task_record_tool "$task_root/build/opensc/card-public-info" "$task_reader" "$task_expected_atr"
        printf 'Consulta de EF.ATR/EF.DIR terminada; revisar resultados y aplicaciones del informe.\n'
    fi
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
