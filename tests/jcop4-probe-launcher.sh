#!/usr/bin/env bash
# Exercise the inventory boundary without a reader or card.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/build/opensc/src/tools"
cp "$task_root/scripts/probe-jcop4.sh" "$task_tmp/scripts/"
cat > "$task_tmp/scripts/build-card-public-info.sh" <<'BUILD'
#!/usr/bin/env bash
set -euo pipefail
if [[ ${FIXTURE_PUBLIC_BUILD_EXIT:-0} != 0 ]]; then exit "$FIXTURE_PUBLIC_BUILD_EXIT"; fi
cat > "$FIXTURE_PROBE_ROOT/build/opensc/card-public-info" <<'HELPER'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 2 && $1 == 2 && $2 == 3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a ]] || exit 97
[[ ${OPENSC_DEBUG-} == 0 && ${OPENSC_API_TRACE_FILE-unset} == unset &&
   ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE-unset} == unset &&
   $(stat -c %a "$OPENSC_CONF") == 600 ]] || exit 98
if IFS= read -r task_input; then exit 99; fi
printf 'public-files\n' > "$FIXTURE_PROBE_ROOT/public-called"
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
export OPENSC_JCOP3_SESSION_STATE=yes OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes
export OPENSC_JCOP3_POST_LOGIN_STATUS=yes OPENSC_JCOP3_POST_LOGIN_SELECT=yes
task_run() {
    local task_expected=$1 task_rc=0
    shift
    rm -f "$task_tmp/called"
    rm -f "$task_tmp/public-called"
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
mv "$task_tmp/build/opensc/src/tools/opensc-tool" "$task_tmp/saved-tool"
task_run 1
[[ ! -e $task_tmp/called && $(< "$task_tmp/output.txt") == *'make build/opensc'* ]]
printf 'PASS JCOP4 probe requires the local build instead of falling back to system OpenSC\n'
