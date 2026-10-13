#!/usr/bin/env bash
# Exercise the inventory boundary without a reader or card.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/build/opensc/src/tools"
cp "$task_root/scripts/probe-jcop4.sh" "$task_tmp/scripts/"
# Replace only the fixed installed-module location in the copied fixture script.
task_fixture_script=$(< "$task_tmp/scripts/probe-jcop4.sh")
task_fixture_script=${task_fixture_script/'task_idopte_module=/run/current-system/sw/lib/SCMiddleware/libidop11.so'/'task_idopte_module="$task_root/fixture-idopte.so"'}
printf '%s\n' "$task_fixture_script" > "$task_tmp/scripts/probe-jcop4.sh"
touch "$task_tmp/fixture-idopte.so"
cat > "$task_tmp/scripts/build-pcsc-public-observer.sh" <<'OBSERVER'
#!/usr/bin/env bash
set -euo pipefail
exit_status=${FIXTURE_OBSERVER_BUILD_EXIT:-0}
if (( exit_status )); then exit "$exit_status"; fi
mkdir -p "$FIXTURE_PROBE_ROOT/build/diagnostics"
# The loader ignores this empty fixture; the tool mock below simulates events.
touch "$FIXTURE_PROBE_ROOT/build/diagnostics/pcsc-public-observer.so"
OBSERVER
cat > "$task_tmp/build/opensc/src/tools/pkcs11-tool" <<'PKCS11'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 4 && $1 == --module && $2 == "$FIXTURE_PROBE_ROOT/fixture-idopte.so" &&
   $3 == --show-info && $4 == --list-slots ]] || exit 97
[[ ${OPENSC_DEBUG-} == 0 && ${OPENSC_API_TRACE_FILE-unset} == unset &&
   ${OPENSC_DEBUG_FILE-unset} == unset && ${OPENSC_DEEPBIND-unset} == unset &&
   $(stat -c %a "$OPENSC_CONF") == 600 ]] || exit 98
if IFS= read -r task_input; then exit 99; fi
if [[ ${JCOP4_PUBLIC_DISCOVERY_TRACE-unset} != unset ]]; then
    [[ ${LD_PRELOAD-unset} == unset &&
       ${LIBPCSCLITE_DELEGATE-} == "$FIXTURE_PROBE_ROOT/build/diagnostics/pcsc-public-observer.so" &&
       ${LIBPCSCLITE_SPY_DELEGATE-unset} == unset ]] || exit 96
    printf 'format=public_discovery_headers_v2\n' > "$JCOP4_PUBLIC_DISCOVERY_TRACE"
    if [[ ${FIXTURE_OBSERVER_LIFECYCLE_ONLY:-0} == 1 ]]; then
        printf 'pcsc_call=SCardConnect\npcsc_result=2148532235\n' >> "$JCOP4_PUBLIC_DISCOVERY_TRACE"
    elif [[ ${FIXTURE_OBSERVER_NO_EVENTS:-0} == 0 ]]; then
        printf 'public_header=00A40400\nsw=9000\nblocked_header=00CA0101\n' >> "$JCOP4_PUBLIC_DISCOVERY_TRACE"
    fi
else
    [[ ${LIBPCSCLITE_DELEGATE-unset} == unset && ${LIBPCSCLITE_SPY_DELEGATE-unset} == unset ]] || exit 96
fi
printf '%s\n' "$*" > "$FIXTURE_PROBE_ROOT/idopte-called"
printf 'Idopte fixture\n  token flags: PIN initialized\n'
exit "${FIXTURE_IDOPTE_EXIT:-0}"
PKCS11
chmod +x "$task_tmp/build/opensc/src/tools/pkcs11-tool"
cat > "$task_tmp/scripts/build-card-public-info.sh" <<'BUILD'
#!/usr/bin/env bash
set -euo pipefail
if [[ ${FIXTURE_PUBLIC_BUILD_EXIT:-0} != 0 ]]; then exit "$FIXTURE_PUBLIC_BUILD_EXIT"; fi
cat > "$FIXTURE_PROBE_ROOT/build/opensc/card-public-info" <<'HELPER'
#!/usr/bin/env bash
set -euo pipefail
[[ ($# == 2 || $# == 4) && $1 == 2 && $2 == 3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a ]] || exit 97
if (( $# == 4 )); then
    [[ ($3 == --pkcs15 || $3 == --sign-setup || $3 == --pin-status || $3 == --security-info || $3 == --pin-policy) && $4 == E828BD080FA00000036364796E6964 ]] || exit 97
fi
[[ ${OPENSC_DEBUG-} == 0 && ${OPENSC_API_TRACE_FILE-unset} == unset &&
   ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE-unset} == unset &&
   $(stat -c %a "$OPENSC_CONF") == 600 ]] || exit 98
if IFS= read -r task_input; then exit 99; fi
printf '%s\n' "$*" > "$FIXTURE_PROBE_ROOT/public-called"
printf 'ef_atr_result=0\nef_dir_result=0\napplication_count=1\n'
exit "${FIXTURE_PUBLIC_EXIT:-0}"
HELPER
chmod +x "$FIXTURE_PROBE_ROOT/build/opensc/card-public-info"
BUILD
cat > "$task_tmp/bin/git" <<'GIT'
#!/usr/bin/env bash
case ${3-} in
    rev-parse) printf '%040d\n' 1 ;;
    status) exit 0 ;;
    *) exit 91 ;;
esac
GIT
cat > "$task_tmp/build/opensc/src/tools/opensc-tool" <<'TOOL'
#!/usr/bin/env bash
set -euo pipefail
case "$*" in
    --info|--list-readers|'--reader 2 --atr') ;;
    '--reader 2 --card-driver default --send-apdu 00A404040BF0496173456363526F6F7400'|\
    '--reader 2 --card-driver default --send-apdu 00A404040EE828BD080FD25047656E6572696300'|\
    '--reader 2 --card-driver default --send-apdu 00A404040AE828BD080F014E58503100'|\
    '--reader 2 --card-driver default --send-apdu 00A404040AE828BD080F014E58503000') ;;
    *) exit 92 ;;
esac
[[ ${OPENSC_DEBUG-} == 0 && ${OPENSC_DEBUG_FILE-unset} == unset &&
   ${OPENSC_DEEPBIND-unset} == unset &&
   ${LD_PRELOAD-unset} == unset && ${LIBPCSCLITE_DELEGATE-unset} == unset &&
   ${LIBPCSCLITE_SPY_DELEGATE-unset} == unset &&
   ${OPENSC_API_TRACE_FILE-unset} == unset &&
   ${OPENSC_JCOP3_SESSION_STATE-unset} == unset &&
   ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE-unset} == unset &&
   ${OPENSC_JCOP3_POST_LOGIN_STATUS-unset} == unset &&
   ${OPENSC_JCOP3_POST_LOGIN_SELECT-unset} == unset ]] || exit 93
[[ $(stat -c %a "$OPENSC_CONF") == 600 &&
   $(< "$OPENSC_CONF") == *'debug = 0;'* &&
   $(< "$OPENSC_CONF") == *'use_pin_caching = no;'* &&
   $(< "$OPENSC_CONF") != *'try_emulation_first'* ]] || exit 94
if IFS= read -r task_input; then exit 95; fi
printf '%s\n' "$*" >> "$FIXTURE_PROBE_ROOT/called"
if [[ $* == "${FIXTURE_PROBE_FAIL-}" ]]; then exit 17; fi
case "$*" in
    --info) printf 'OpenSC fixture\n' ;;
    --list-readers) printf '0 No; 2 Yes fixture reader\n' ;;
    '--reader 2 --atr') printf '%s\n' "${FIXTURE_PROBE_ATR:-3b:00}" ;;
    *)
        case ${FIXTURE_PROBE_SW-mixed} in
            mixed)
                if [[ $6 == *E828BD080FD25047656E6572696300 ]]; then
                    printf 'Received (SW1=0x90, SW2=0x00):\n'
                else
                    printf 'Received (SW1=0x6A, SW2=0x82)\n'
                fi ;;
            denied) printf 'Received (SW1=0x69, SW2=0x82)\n' ;;
            missing) printf 'No response.\n' ;;
            duplicate) printf 'Received (SW1=0x90, SW2=0x00)\nReceived (SW1=0x90, SW2=0x00)\n' ;;
            *) exit 96 ;;
        esac ;;
esac
TOOL
chmod +x "$task_tmp/bin/git" "$task_tmp/build/opensc/src/tools/opensc-tool"
export PATH="$task_tmp/bin:$PATH" FIXTURE_PROBE_ROOT="$task_tmp" READER=2
export OPENSC_DEBUG=99 OPENSC_DEBUG_FILE=/should/not/be/used
export OPENSC_CONF=/should/not/be/used OPENSC_API_TRACE_FILE=/should/not/be/used
export OPENSC_DEEPBIND=1
export LIBPCSCLITE_DELEGATE=/should/not/be/used LIBPCSCLITE_SPY_DELEGATE=/should/not/be/used
export OPENSC_JCOP3_SESSION_STATE=yes OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes
export OPENSC_JCOP3_POST_LOGIN_STATUS=yes OPENSC_JCOP3_POST_LOGIN_SELECT=yes
task_run() {
    local task_expected=$1 task_rc=0
    shift
    rm -f "$task_tmp/called"
    rm -f "$task_tmp/public-called"
    rm -f "$task_tmp/idopte-called"
    bash "$task_tmp/scripts/probe-jcop4.sh" "$@" > "$task_tmp/output.txt" 2>&1 || task_rc=$?
    [[ $task_rc == "$task_expected" ]]
}
printf 'synthetic unused stdin\n' | task_run 0
[[ $(< "$task_tmp/called") == $'--info\n--list-readers\n--reader 2 --atr' ]]
[[ $(< "$task_tmp/output.txt") == *'Consulta de lector y ATR terminada.'* ]]
for task_dir in "$task_tmp"/local/jcop4-probe-*; do
    [[ $(stat -c %a "$task_dir") == 700 &&
       $(stat -c %a "$task_dir/report.txt") == 600 &&
       $(< "$task_dir/report.txt") != *'synthetic unused stdin'* &&
       $(< "$task_dir/report.txt") == *'3b:00'* ]]
done
printf 'PASS JCOP4 probe restricts commands to inventory, discards stdin and isolates configuration\n'
export FIXTURE_PROBE_FAIL=--list-readers
task_run 17
[[ $(< "$task_tmp/called") == $'--info\n--list-readers' &&
   $(< "$task_tmp/output.txt") != *'Consulta de lector y ATR terminada.'* ]]
printf 'PASS JCOP4 probe stops on failure without retries or subsequent card queries\n'
READER=invalid task_run 2
[[ ! -e $task_tmp/called ]]
task_run 2 2 unexpected
[[ ! -e $task_tmp/called ]]
printf 'PASS JCOP4 probe rejects invalid reader and additional commands\n'
unset FIXTURE_PROBE_FAIL
task_run 1 2 --aids
[[ $(wc -l < "$task_tmp/called") == 3 &&
   $(< "$task_tmp/output.txt") == *'ATR distinto o ausente'* ]]
printf 'PASS JCOP4 AID query checks the observed ATR before sending SELECT\n'
export FIXTURE_PROBE_ATR=3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a
task_run 0 2 --aids
[[ $(wc -l < "$task_tmp/called") == 7 &&
   $(< "$task_tmp/output.txt") == *'1 de 4 AID seleccionados'* ]]
[[ $(sed -n '4,7p' "$task_tmp/called") == $'--reader 2 --card-driver default --send-apdu 00A404040BF0496173456363526F6F7400\n--reader 2 --card-driver default --send-apdu 00A404040EE828BD080FD25047656E6572696300\n--reader 2 --card-driver default --send-apdu 00A404040AE828BD080F014E58503100\n--reader 2 --card-driver default --send-apdu 00A404040AE828BD080F014E58503000' ]]
for task_dir in "$task_tmp"/local/jcop4-aids-*; do
    [[ $(stat -c %a "$task_dir") == 700 && $(stat -c %a "$task_dir/report.txt") == 600 ]]
done
printf 'PASS JCOP4 AID query sends only four known SELECT and distinguishes 9000 from 6A82\n'
export FIXTURE_PROBE_SW=denied
task_run 1 2 --aids
[[ $(wc -l < "$task_tmp/called") == 4 && $(< "$task_tmp/output.txt") == *'SELECT devolvió 6982'* ]]
printf 'PASS JCOP4 AID query stops on security rejection despite zero tool exit status\n'
for task_sw_mode in missing duplicate; do
    export FIXTURE_PROBE_SW="$task_sw_mode"
    task_run 1 2 --aids
    [[ $(wc -l < "$task_tmp/called") == 4 ]]
done
printf 'PASS JCOP4 AID query rejects missing or duplicate responses without subsequent SELECT\n'
printf 'synthetic unused stdin\n' | task_run 0 2 --files
[[ $(wc -l < "$task_tmp/called") == 3 && -f $task_tmp/public-called &&
   $(< "$task_tmp/output.txt") == *'Consulta de EF.ATR/EF.DIR terminada'* ]]
printf 'PASS JCOP4 file discovery uses the isolated helper without candidate AID probes or stdin\n'
export FIXTURE_PUBLIC_EXIT=17
task_run 17 2 --files
[[ -f $task_tmp/public-called && $(< "$task_tmp/output.txt") != *'Consulta de EF.ATR/EF.DIR terminada'* ]]
printf 'PASS JCOP4 file discovery propagates helper failure without retries\n'
export FIXTURE_PUBLIC_BUILD_EXIT=19
task_run 19 2 --files
[[ ! -e $task_tmp/public-called && $(wc -l < "$task_tmp/called") == 3 ]]
printf 'PASS JCOP4 file discovery stops before helper execution if compilation fails\n'
unset FIXTURE_PUBLIC_EXIT FIXTURE_PUBLIC_BUILD_EXIT
printf 'synthetic unused stdin\n' | task_run 0 2 --pkcs15
[[ $(wc -l < "$task_tmp/called") == 3 &&
   $(< "$task_tmp/public-called") == '2 3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a --pkcs15 E828BD080FA00000036364796E6964' &&
   $(< "$task_tmp/output.txt") == *'Consulta PKCS#15 pública terminada'* ]]
printf 'PASS JCOP4 PKCS15 discovery binds only the observed AID through the isolated public helper\n'
export FIXTURE_PUBLIC_EXIT=17
task_run 17 2 --pkcs15
[[ -f $task_tmp/public-called && $(< "$task_tmp/output.txt") != *'Consulta PKCS#15 pública terminada'* ]]
printf 'PASS JCOP4 PKCS15 discovery propagates failure without authentication or retries\n'
unset FIXTURE_PUBLIC_EXIT
printf 'synthetic unused stdin\n' | task_run 0 2 --sign-setup
[[ $(wc -l < "$task_tmp/called") == 3 &&
   $(< "$task_tmp/public-called") == '2 3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a --sign-setup E828BD080FA00000036364796E6964' &&
   $(< "$task_tmp/output.txt") == *'Selección temporal MSE aceptada'* ]]
printf 'PASS JCOP4 sign setup uses only the bounded helper with isolated configuration and empty stdin\n'
export FIXTURE_PUBLIC_EXIT=17
task_run 17 2 --sign-setup
[[ -f $task_tmp/public-called && $(< "$task_tmp/output.txt") != *'Selección temporal MSE aceptada'* ]]
printf 'PASS JCOP4 sign setup does not announce success or retry after helper failure\n'
unset FIXTURE_PUBLIC_EXIT
printf 'synthetic unused stdin\n' | task_run 0 2 --pin-status
[[ $(wc -l < "$task_tmp/called") == 3 &&
   $(< "$task_tmp/public-called") == '2 3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a --pin-status E828BD080FA00000036364796E6964' &&
   $(< "$task_tmp/output.txt") == *'VERIFY vacío terminada'* ]]
printf 'PASS JCOP4 PIN status uses the bounded public helper with isolated configuration and empty stdin\n'
export FIXTURE_PUBLIC_EXIT=17
task_run 17 2 --pin-status
[[ -f $task_tmp/public-called && $(< "$task_tmp/output.txt") != *'VERIFY vacío terminada'* ]]
printf 'PASS JCOP4 PIN status stops on helper failure without retries or misleading success\n'
unset FIXTURE_PUBLIC_EXIT
printf 'synthetic unused stdin\n' | task_run 0 2 --security-info
[[ $(wc -l < "$task_tmp/called") == 3 &&
   $(< "$task_tmp/public-called") == '2 3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a --security-info E828BD080FA00000036364796E6964' &&
   $(< "$task_tmp/output.txt") == *'Consulta de metadatos de seguridad terminada, sin PIN ni autenticación'* ]]
for task_dir in "$task_tmp"/local/jcop4-security-info-*; do
    [[ $(stat -c %a "$task_dir") == 700 && $(stat -c %a "$task_dir/report.txt") == 600 ]]
done
printf 'PASS JCOP4 security discovery isolates configuration, uses the observed profile and discards stdin\n'
export FIXTURE_PUBLIC_EXIT=17
task_run 17 2 --security-info
[[ -f $task_tmp/public-called && $(< "$task_tmp/output.txt") != *'Consulta de metadatos de seguridad terminada'* ]]
printf 'PASS JCOP4 security discovery propagates helper failure without retries or misleading success\n'
unset FIXTURE_PUBLIC_EXIT
printf 'synthetic unused stdin\n' | task_run 0 2 --pin-policy
[[ $(wc -l < "$task_tmp/called") == 3 &&
   $(< "$task_tmp/public-called") == '2 3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a --pin-policy E828BD080FA00000036364796E6964' &&
   $(< "$task_tmp/output.txt") == *'Consulta de política pública del PIN terminada, sin PIN ni autenticación'* ]]
for task_dir in "$task_tmp"/local/jcop4-pin-policy-*; do
    [[ $(stat -c %a "$task_dir") == 700 && $(stat -c %a "$task_dir/report.txt") == 600 ]]
done
printf 'PASS JCOP4 PIN policy uses only the isolated helper and observed AID with empty stdin and private reports\n'
export FIXTURE_PUBLIC_EXIT=17
task_run 17 2 --pin-policy
[[ -f $task_tmp/public-called && $(< "$task_tmp/output.txt") != *'Consulta de política pública del PIN terminada'* ]]
printf 'PASS JCOP4 PIN policy propagates errors without retries or success announcement\n'
unset FIXTURE_PUBLIC_EXIT
printf 'synthetic unused stdin\n' | task_run 0 2 --idopte
[[ $(wc -l < "$task_tmp/called") == 3 && ! -e $task_tmp/public-called &&
   -f $task_tmp/idopte-called &&
   $(< "$task_tmp/output.txt") == *'Consulta pública de Idopte terminada, sin login ni PIN'* &&
   $(< "$task_tmp/output.txt") != *'token flags'* ]]
for task_dir in "$task_tmp"/local/jcop4-idopte-*; do
    [[ $(stat -c %a "$task_dir") == 700 && $(stat -c %a "$task_dir/report.txt") == 600 &&
       $(< "$task_dir/report.txt") != *'synthetic unused stdin'* ]]
done
printf 'PASS JCOP4 Idopte query requests only module/slot/token metadata with empty stdin and private reports\n'
export FIXTURE_IDOPTE_EXIT=17
task_run 17 2 --idopte
[[ -f $task_tmp/idopte-called && $(< "$task_tmp/output.txt") != *'Consulta pública de Idopte terminada'* ]]
printf 'PASS JCOP4 Idopte query propagates module failure without retries or success announcement\n'
unset FIXTURE_IDOPTE_EXIT
export FIXTURE_PROBE_ATR=3b:00
task_run 1 2 --idopte
[[ ! -e $task_tmp/idopte-called && $(wc -l < "$task_tmp/called") == 3 ]]
printf 'PASS JCOP4 Idopte query checks the observed ATR before loading the proprietary module\n'
export FIXTURE_PROBE_ATR=3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a
task_run 0 2 --idopte-discovery
[[ -f $task_tmp/idopte-called && ! -e $task_tmp/public-called &&
   $(< "$task_tmp/output.txt") == *'Descubrimiento público observado'* ]]
for task_dir in "$task_tmp"/local/jcop4-idopte-discovery-*; do
    [[ $(stat -c %a "$task_dir") == 700 && $(stat -c %a "$task_dir/public-headers.txt") == 600 &&
       $(< "$task_dir/report.txt") == *'observer_result=events_recorded'* ]]
done
printf 'PASS JCOP4 discovery enables the guard only for public Idopte inventory and records private header events\n'
export FIXTURE_IDOPTE_EXIT=17
task_run 17 2 --idopte-discovery
[[ $(< "$task_tmp/output.txt") == *'Descubrimiento público observado'* ]]
printf 'PASS JCOP4 discovery preserves header evidence and module failure without retrying\n'
unset FIXTURE_IDOPTE_EXIT
export FIXTURE_OBSERVER_NO_EVENTS=1
task_run 1 2 --idopte-discovery
[[ $(< "$task_tmp/output.txt") == *'No se observaron llamadas PC/SC'* ]]
printf 'PASS JCOP4 discovery reports absent interposition events instead of claiming success\n'
unset FIXTURE_OBSERVER_NO_EVENTS
export FIXTURE_OBSERVER_LIFECYCLE_ONLY=1
task_run 1 2 --idopte-discovery
[[ $(< "$task_tmp/output.txt") == *'Se observaron conexiones PC/SC, pero no APDU'* ]]
task_lifecycle_report=$(sed -n 's/^Informe: //p' "$task_tmp/output.txt")
[[ $(< "$task_lifecycle_report") == *'observer_result=pcsc_lifecycle_only'* &&
   $(< "$task_lifecycle_report") == *'pcsc_call=SCardConnect'* &&
   $(< "$task_lifecycle_report") == *'module_exit_status=0'* ]]
printf 'PASS JCOP4 discovery preserves lifecycle failures without claiming APDU observation or falling back\n'
unset FIXTURE_OBSERVER_LIFECYCLE_ONLY
export FIXTURE_OBSERVER_BUILD_EXIT=19
task_run 19 2 --idopte-discovery
[[ ! -e $task_tmp/idopte-called && $(wc -l < "$task_tmp/called") == 3 ]]
printf 'PASS JCOP4 discovery stops before Idopte if observer compilation fails\n'
unset FIXTURE_OBSERVER_BUILD_EXIT
mv "$task_tmp/fixture-idopte.so" "$task_tmp/saved-idopte.so"
task_run 1 2 --idopte
[[ ! -e $task_tmp/idopte-called && ! -e $task_tmp/public-called ]]
mv "$task_tmp/saved-idopte.so" "$task_tmp/fixture-idopte.so"
mv "$task_tmp/build/opensc/src/tools/pkcs11-tool" "$task_tmp/saved-pkcs11-tool"
task_run 1 2 --idopte
[[ ! -e $task_tmp/idopte-called && ! -e $task_tmp/public-called ]]
printf 'PASS JCOP4 Idopte query stops if the fixed module or local tool is absent without fallback\n'
mv "$task_tmp/build/opensc/src/tools/opensc-tool" "$task_tmp/saved-tool"
task_run 1
[[ ! -e $task_tmp/called && $(< "$task_tmp/output.txt") == *'make build/opensc'* ]]
printf 'PASS JCOP4 probe requires the local build instead of falling back to system OpenSC\n'
