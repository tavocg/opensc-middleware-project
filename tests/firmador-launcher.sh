#!/usr/bin/env bash
# No real PIN, Java, reader or card. The stdin fixture is synthetic.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/build/opensc/src/pkcs11/.libs" "$task_tmp/local"
cp "$task_root/scripts/firmador-jcop3.sh" "$task_tmp/scripts/"
touch "$task_tmp/build/opensc/src/pkcs11/.libs/opensc-pkcs11.so" "$task_tmp/input with spaces.pdf"

cat > "$task_tmp/bin/git" <<'GIT'
#!/usr/bin/env bash
case ${3-} in
    rev-parse) printf '%040d\n' 1 ;;
    status) exit 0 ;;
    *) exit 91 ;;
esac
GIT
cat > "$task_tmp/bin/firmador" <<'FIRMADOR'
#!/usr/bin/env bash
set -euo pipefail
[[ ${OPENSC_DEBUG-} == 0 && ${OPENSC_JCOP3_POST_LOGIN_STATUS-unset} == unset &&
   ${OPENSC_JCOP3_POST_LOGIN_SELECT-unset} == unset ]] || exit 92
[[ -f $LIBASEP11 && -f $OPENSC_CONF ]] || exit 93
task_config=$(< "$OPENSC_CONF")
[[ $task_config == *'use_pin_caching = no;'* &&
   $task_config == *'try_emulation_first = yes;'* && $task_config == *'debug = 0;'* ]] || exit 94
[[ $(stat -c %a "$OPENSC_CONF") == 600 ]] || exit 95
[[ ${OPENSC_JCOP3_SESSION_STATE-unset} == "${FIXTURE_SESSION-unset}" &&
   ${OPENSC_JCOP3_PIN_STATUS_IN_PLACE-unset} == "${FIXTURE_CANDIDATE-unset}" ]] || exit 96
if [[ ${FIXTURE_TRACE-no} == yes ]]; then
    [[ -f $OPENSC_API_TRACE_FILE && $(stat -c %a "$OPENSC_API_TRACE_FILE") == 600 &&
       $(stat -c %a "$(dirname "$OPENSC_API_TRACE_FILE")") == 700 ]] || exit 97
else
    [[ ${OPENSC_API_TRACE_FILE-unset} == unset ]] || exit 98
fi
if [[ ${FIXTURE_MODE-gui} == cli ]]; then
    [[ $# == 3 && $1 == -dargs && $2 == "$FIXTURE_ROOT/input with spaces.pdf" &&
       $3 == "$FIXTURE_ROOT/local/result with spaces.pdf" ]] || exit 99
    IFS= read -r task_synthetic
    [[ $task_synthetic == fixture-stdin ]] || exit 100
else
    [[ $# == 0 ]] || exit 101
fi
: > "$FIXTURE_ROOT/called"
exit 17
FIRMADOR
chmod +x "$task_tmp/bin/git" "$task_tmp/bin/firmador"
export PATH="$task_tmp/bin:$PATH" FIXTURE_ROOT="$task_tmp"
export OPENSC_DEBUG=99 OPENSC_API_TRACE_FILE=/should/not/be/used
export OPENSC_JCOP3_SESSION_STATE=yes OPENSC_JCOP3_PIN_STATUS_IN_PLACE=yes
export OPENSC_JCOP3_POST_LOGIN_STATUS=yes OPENSC_JCOP3_POST_LOGIN_SELECT=yes

task_run() {
    local task_rc=0
    bash "$task_tmp/scripts/firmador-jcop3.sh" "$@" > "$task_tmp/output.txt" 2>&1 || task_rc=$?
    [[ $task_rc == 17 && -f "$task_tmp/called" ]]
    rm -f "$task_tmp/called"
}
export FIXTURE_MODE=cli FIXTURE_CANDIDATE=yes FIXTURE_SESSION=unset FIXTURE_TRACE=yes
printf 'fixture-stdin\n' | task_run --cli 'input with spaces.pdf' 'local/result with spaces.pdf'
printf 'PASS CLI forwards PDF arguments and stdin, preserves private configuration and exit status\n'
task_reports=("$task_tmp"/local/jcop3-firmador-api-*/report.txt)
task_report=$(< "${task_reports[0]}")
[[ $task_report == *'launch_mode=cli'* && $task_report == *'pin_verify_experiment=fresh_verify'* &&
   $task_report != *fixture-stdin* && $(< "$task_tmp/output.txt") != *fixture-stdin* ]]
printf 'PASS CLI records mode and candidate without the synthetic credential\n'

task_invalid() {
    local task_rc=0
    bash "$task_tmp/scripts/firmador-jcop3.sh" --cli "$1" "$2" > "$task_tmp/output.txt" 2>&1 || task_rc=$?
    [[ $task_rc == 2 && ! -e "$task_tmp/called" ]]
}
touch "$task_tmp/local/existing.pdf"
task_invalid 'input with spaces.pdf' local/existing.pdf
printf 'PASS CLI refuses existing output before calling firmador\n'
task_invalid missing.pdf local/new.pdf
printf 'PASS CLI refuses unreadable input before calling firmador\n'
task_invalid 'input with spaces.pdf' missing-directory/new.pdf
printf 'PASS CLI refuses missing output directory before calling firmador\n'

export FIXTURE_MODE=gui FIXTURE_CANDIDATE=unset FIXTURE_TRACE=no
task_run
printf 'PASS GUI normal mode remains unchanged and clears inherited diagnostics\n'
export FIXTURE_TRACE=yes
task_run --trace-api
printf 'PASS GUI diagnostics preserves configuration without candidate opt-ins\n'
export FIXTURE_CANDIDATE=yes
task_run --pin-status-in-place
printf 'PASS GUI candidate keeps physical query and real VERIFY opt-in\n'
export FIXTURE_CANDIDATE=unset FIXTURE_SESSION=yes
task_run --session-state
printf 'PASS legacy GUI experiment remains explicitly selected\n'
