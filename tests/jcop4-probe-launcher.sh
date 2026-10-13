#!/usr/bin/env bash
# Exercise the inventory boundary without a reader or card.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/build/opensc/src/tools"
cp "$task_root/scripts/probe-jcop4.sh" "$task_tmp/scripts/"
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
    *) printf '3b:00\n' ;;
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
mv "$task_tmp/build/opensc/src/tools/opensc-tool" "$task_tmp/saved-tool"
task_run 1
[[ ! -e $task_tmp/called && $(< "$task_tmp/output.txt") == *'make build/opensc'* ]]
printf 'PASS JCOP4 probe requires the local build instead of falling back to system OpenSC\n'
