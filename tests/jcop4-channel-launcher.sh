#!/usr/bin/env bash
# Isolated launcher fixtures, no reader/card and no actual timeout wait.
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
mkdir -p "$task_tmp/scripts" "$task_tmp/bin" "$task_tmp/build/opensc"
cp "$task_root/scripts/probe-jcop4-channel.sh" "$task_tmp/scripts/"
cat > "$task_tmp/scripts/build-jcop4-channel-probe.sh" <<'BUILD'
#!/usr/bin/env bash
set -euo pipefail
if [[ ${FIXTURE_BUILD_FAIL:-0} != 0 ]]; then exit 31; fi
task_binary=jcop4-channel-probe
if (( $# )); then
    [[ $# == 2 && -z $1 && $2 == --transport-extended ]] || exit 95
    task_binary=jcop4-channel-transport
fi
cp "$FIXTURE_ROOT/helper" "$FIXTURE_ROOT/build/opensc/$task_binary"
chmod +x "$FIXTURE_ROOT/build/opensc/$task_binary"
BUILD
cat > "$task_tmp/helper" <<'HELPER'
#!/usr/bin/env bash
set -euo pipefail
[[ $1 == 0 && $OPENSC_DEBUG == 0 && $(stat -c %a "$OPENSC_CONF") == 600 ]] || exit 91
if [[ ${FIXTURE_EXPECT_TRANSPORT:-0} == 1 ]]; then
    [[ $# == 2 && $2 == --transport-extended ]] || exit 91
elif [[ ${FIXTURE_EXPECT_CONTEXT:-0} == 1 ]]; then
    [[ $# == 2 && $2 == --pin-context ]] || exit 91
else
    [[ $# == 1 ]] || exit 91
fi
for task_name in OPENSC_API_TRACE_FILE OPENSC_DEBUG_FILE OPENSC_DEEPBIND LD_PRELOAD LD_AUDIT LD_LIBRARY_PATH \
    LIBPCSCLITE_DELEGATE LIBPCSCLITE_SPY_DELEGATE JCOP4_PUBLIC_DISCOVERY_TRACE JCOP4_PUBLIC_SELECT_SPY_TRACE; do
    [[ ! -v $task_name ]] || exit 92
done
if IFS= read -r task_input; then exit 93; fi
printf 'called\n' >> "$FIXTURE_ROOT/invocations"
printf 'channel_result=synthetic_fixture_no_hardware\nfinal_reset_result=0\n'
exit "${FIXTURE_HELPER_EXIT:-0}"
HELPER
cat > "$task_tmp/bin/timeout" <<'TIMEOUT'
#!/usr/bin/env bash
set -euo pipefail
[[ $1 == --signal=TERM && $2 == --kill-after=5s && $3 == 45s ]] || exit 94
shift 3
exec "$@"
TIMEOUT
cat > "$task_tmp/bin/git" <<'GIT'
#!/usr/bin/env bash
exit 0
GIT
chmod +x "$task_tmp/bin/timeout" "$task_tmp/bin/git" "$task_tmp/helper"
export FIXTURE_ROOT="$task_tmp"
export PATH="$task_tmp/bin:$PATH"
run_fixture() {
    local task_expected=$1 task_status=0
    shift
    printf 'SYNTHETIC_INPUT_SHOULD_NOT_BE_READ\n' | bash "$task_tmp/scripts/probe-jcop4-channel.sh" 0 "$@" > "$task_tmp/stdout" 2> "$task_tmp/stderr" || task_status=$?
    [[ $task_status == "$task_expected" ]]
    task_report=$(sed -n 's/^Informe: //p' "$task_tmp/stdout")
    [[ -f $task_report && $(stat -c %a "$task_report") == 600 && $(stat -c %a "${task_report%/*}") == 700 ]]
    [[ $(stat -c %a "${task_report%/*}/opensc.conf") == 600 ]]
}
export OPENSC_API_TRACE_FILE=unused OPENSC_DEBUG_FILE=unused OPENSC_DEEPBIND=unused OPENSC_DEBUG=9
export LIBPCSCLITE_DELEGATE=unused LIBPCSCLITE_SPY_DELEGATE=unused
export JCOP4_PUBLIC_DISCOVERY_TRACE=unused JCOP4_PUBLIC_SELECT_SPY_TRACE=unused
run_fixture 0
[[ $(wc -l < "$task_tmp/invocations") == 1 ]]
rg -q '^helper_exit_status=0$' "$task_report"
printf 'PASS launcher isolates tracing/delegates/debug, closes stdin and creates private reports/configuration\n'
export FIXTURE_HELPER_EXIT=1
run_fixture 1
[[ $(wc -l < "$task_tmp/invocations") == 2 ]]
rg -q '^helper_exit_status=1$' "$task_report"
export FIXTURE_HELPER_EXIT=124
run_fixture 124
[[ $(wc -l < "$task_tmp/invocations") == 3 ]]
rg -q '^helper_exit_status=124$' "$task_report"
printf 'PASS failure/timeout status is preserved with one invocation and no fallback\n'
export FIXTURE_BUILD_FAIL=1
run_fixture 31
[[ $(wc -l < "$task_tmp/invocations") == 3 ]]
rg -q '^helper_build_status=31$' "$task_report"
printf 'PASS build failure prevents helper execution\n'
if bash "$task_tmp/scripts/probe-jcop4-channel.sh" -1 > /dev/null 2>&1; then exit 1; fi
if bash "$task_tmp/scripts/probe-jcop4-channel.sh" 0 extra > /dev/null 2>&1; then exit 1; fi
[[ $(wc -l < "$task_tmp/invocations") == 3 ]]
printf 'PASS invalid reader/extra argument is rejected before execution\n'
unset FIXTURE_BUILD_FAIL
export FIXTURE_HELPER_EXIT=0 FIXTURE_EXPECT_CONTEXT=1
run_fixture 0 --pin-context
[[ $(wc -l < "$task_tmp/invocations") == 4 && $task_report == *'/jcop4-channel-pin-context-'* ]]
rg -q '^mode=anonymous_channel_pin_context_no_pin_once$' "$task_report"
printf 'PASS explicit context flag selects isolated private report and helper argument, still with empty stdin and no inherited tracing\n'
export FIXTURE_HELPER_EXIT=1
run_fixture 1 --pin-context
[[ $(wc -l < "$task_tmp/invocations") == 5 ]]
rg -q '^helper_exit_status=1$' "$task_report"
unset FIXTURE_EXPECT_CONTEXT
export FIXTURE_HELPER_EXIT=0 JCOP4_CHANNEL_PIN_CONTEXT=1
run_fixture 0
[[ $(wc -l < "$task_tmp/invocations") == 6 ]]
rg -q '^mode=anonymous_channel_no_pin_once$' "$task_report"
printf 'PASS failed context run is never retried and an environment flag alone cannot enable the opt-in mode\n'
export FIXTURE_EXPECT_TRANSPORT=1
for task_exit in 0 1 124; do
    export FIXTURE_HELPER_EXIT="$task_exit"
    run_fixture "$task_exit" --transport-extended
    [[ $task_report == *'/jcop4-channel-transport-'* ]]
    rg -q '^mode=anonymous_channel_extended_mf_no_pin_once$' "$task_report"
    rg -q '^policy=candidate_t1_read172_mf_extended291$' "$task_report"
done
[[ $(wc -l < "$task_tmp/invocations") == 9 ]]
printf 'PASS extended transport requires an explicit argument/separate binary/private prefix and preserves0/1/124 with one no-PIN invocation\n'
unset FIXTURE_EXPECT_TRANSPORT
export FIXTURE_HELPER_EXIT=0 JCOP4_CHANNEL_TRANSPORT=1
run_fixture 0
[[ $(wc -l < "$task_tmp/invocations") == 10 ]]
rg -q '^mode=anonymous_channel_no_pin_once$' "$task_report"
rg -q '^policy=opensc_t1_short_max256$' "$task_report"
printf 'PASS an environment transport flag cannot enable the experimental profile or change legacy policy\n'
