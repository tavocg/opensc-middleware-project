#!/usr/bin/env bash
# Check the public SELECT helper with a fake tool; no reader, PIN or APDU.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/build/opensc/src/tools"
cp "$task_root/scripts/peer-context-jcop3.sh" "$task_tmp/scripts/"
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
[[ $# == 6 && $1 == --reader && $2 == 2 && $3 == --card-driver &&
   $4 == default && $5 == --send-apdu && $6 == 00A404040BF0496173456363526F6F7400 ]] || exit 92
[[ ${OPENSC_DEBUG-} == 0 && ${OPENSC_API_TRACE_FILE-unset} == unset &&
   ${OPENSC_JCOP3_SESSION_STATE-unset} == unset &&
   ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE-unset} == unset &&
   ${OPENSC_JCOP3_POST_LOGIN_STATUS-unset} == unset &&
   ${OPENSC_JCOP3_POST_LOGIN_SELECT-unset} == unset ]] || exit 93
[[ $(stat -c %a "$OPENSC_CONF") == 600 &&
   $(< "$OPENSC_CONF") == *'debug = 0;'* &&
   $(< "$OPENSC_CONF") == *'use_pin_caching = no;'* ]] || exit 94
if IFS= read -r task_input; then exit 95; fi
printf 'call\n' >> "$FIXTURE_PEER_ROOT/called"
case ${FIXTURE_PEER_SW-ok} in
    ok) printf 'Received (SW1=0x90, SW2=0x00):\n' ;;
    reject) printf 'Received (SW1=0x69, SW2=0x82)\n' ;;
    missing) printf 'No response.\n' ;;
esac
exit "${FIXTURE_PEER_EXIT:-0}"
TOOL
chmod +x "$task_tmp/bin/git" "$task_tmp/build/opensc/src/tools/opensc-tool"
export PATH="$task_tmp/bin:$PATH" FIXTURE_PEER_ROOT="$task_tmp" READER=2
export OPENSC_DEBUG=99 OPENSC_API_TRACE_FILE=/should/not/be/used
export OPENSC_JCOP3_SESSION_STATE=yes OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes
export OPENSC_JCOP3_POST_LOGIN_STATUS=yes OPENSC_JCOP3_POST_LOGIN_SELECT=yes
task_run() {
    local task_expected=$1 task_rc=0
    shift
    rm -f "$task_tmp/called"
    bash "$task_tmp/scripts/peer-context-jcop3.sh" "$@" > "$task_tmp/output.txt" 2>&1 || task_rc=$?
    [[ $task_rc == "$task_expected" ]]
    if [[ -e $task_tmp/called ]]; then
        [[ $(< "$task_tmp/called") == call ]]
    fi
}
printf 'synthetic unused stdin\n' | task_run 0
[[ -f $task_tmp/called && $(< "$task_tmp/output.txt") == *'SELECT raíz IAS: 9000.'* ]]
for task_report in "$task_tmp"/local/jcop3-peer-context-*.txt; do
    [[ $(stat -c %a "$task_report") == 600 &&
       $(< "$task_report") != *'synthetic unused stdin'* ]]
done
printf 'PASS peer helper sends only public SELECT, discards stdin and overrides inherited diagnostics\n'
export FIXTURE_PEER_EXIT=17
task_run 17
[[ -f $task_tmp/called ]]
printf 'PASS peer helper propagates transport failure without retry\n'
export FIXTURE_PEER_EXIT=0 FIXTURE_PEER_SW=reject
task_run 1
[[ -f $task_tmp/called && $(< "$task_tmp/output.txt") != *'SELECT raíz IAS: 9000.'* ]]
printf 'PASS peer helper rejects SW 6982 even when tool exits zero\n'
export FIXTURE_PEER_SW=missing
task_run 1
[[ -f $task_tmp/called ]]
printf 'PASS peer helper rejects missing status word\n'
READER=invalid task_run 2
[[ ! -e $task_tmp/called ]]
task_run 2 unexpected
[[ ! -e $task_tmp/called ]]
printf 'PASS peer helper rejects invalid reader and extra arguments before accessing tool\n'
mv "$task_tmp/build/opensc/src/tools/opensc-tool" "$task_tmp/saved-tool"
task_run 1
[[ ! -e $task_tmp/called ]]
printf 'PASS peer helper requires compiled tool before creating a new card query\n'
