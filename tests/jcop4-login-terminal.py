"""Synthetic PTY input and launcher fixtures only, no card/context execution."""
import fcntl
import os
from pathlib import Path
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import termios
import time


def pty_run(argv, payload=None, interrupt=False, tty_stdout=False, environment=None, terminal_interrupt=False):
    master, slave = os.openpty()
    original = termios.tcgetattr(slave)

    def terminal():
        os.setsid()
        fcntl.ioctl(slave, termios.TIOCSCTTY, 0)

    proc = subprocess.Popen(argv, stdin=slave, stdout=slave if tty_stdout else subprocess.PIPE,
                            stderr=slave, env=environment, preexec_fn=terminal)
    output = b""
    try:
        limit = time.monotonic() + 8
        while time.monotonic() < limit:
            ready, _, _ = select.select([master], [], [], 0.1)
            if ready:
                output += os.read(master, 4096)
                if b"sin eco" in output:
                    assert not termios.tcgetattr(slave)[3] & termios.ECHO
                    break
            if proc.poll() is not None:
                break
        if payload is not None:
            assert b"sin eco" in output
            if interrupt:
                proc.send_signal(signal.SIGTERM)
            elif terminal_interrupt:
                os.write(master, b"\x03")
            else:
                os.write(master, payload)
        result = proc.wait(timeout=8)
        assert termios.tcgetattr(slave)[3] & termios.ECHO == original[3] & termios.ECHO
        while select.select([master], [], [], 0)[0]:
            output += os.read(master, 4096)
        if proc.stdout is not None:
            output += proc.stdout.read()
        assert b"77777777" not in output
        return result, output
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        os.close(master)
        os.close(slave)


binary = sys.argv[1]
binary = str(Path(binary).resolve())
for payload, expected in ((b"77777777\n", 0), (b"77777\n", 1), (b"7" * 15 + b"\n", 1), (b"77\xc3\xa177777\n", 1)):
    status, output = pty_run([binary], payload)
    assert status == expected and b"input_result=" in output
status, output = pty_run([binary], b"", interrupt=True)
assert status == 1 and b"input_result=1" in output
print("PASS real terminal input on synthetic PTY hides ASCII input, rejects lengths/non-ASCII and restores echo after success/error/TERM")

# Real GNU timeout must be nested under a shell which stays in the terminal
# foreground, as in the actual launcher. A direct timeout session leader or
# an exec-only fake timeout misses the original job-control suspension.
real_timeout = shutil.which("timeout")
assert real_timeout
with tempfile.TemporaryDirectory() as directory:
    nested = Path(directory) / "nested.sh"
    nested.write_text('''#!/usr/bin/env bash
set -u
"$1" --signal=TERM --kill-after=1s 2s "$2" </dev/null
task_status=$?
exit "$task_status"
''')
    status, output = pty_run(["bash", str(nested), real_timeout, binary])
    assert status == 124 and b"sin eco" not in output and b"input_result=1" in output
    nested.write_text(nested.read_text().replace('"$1" --signal', '"$1" --foreground --signal'))
    status, output = pty_run(["bash", str(nested), real_timeout, binary], b"77777777\n")
    assert status == 0 and b"input_result=0" in output
    status, output = pty_run(["bash", str(nested), real_timeout, binary], b"", terminal_interrupt=True)
    assert status != 124 and status != 0 and b"input_result=1" in output
    status, output = pty_run(["bash", str(nested), real_timeout, binary])
    assert status == 124 and b"sin eco" in output and b"input_result=1" in output
print("PASS nested real GNU timeout reproduces missing prompt without foreground; foreground enables hidden input, terminal Ctrl+C and clean timeout")

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    fixture = Path(directory)
    (fixture / "scripts").mkdir()
    (fixture / "bin").mkdir()
    (fixture / "build/opensc").mkdir(parents=True)
    launcher = fixture / "scripts/probe-jcop4-channel.sh"
    launcher.write_text((root / "scripts/probe-jcop4-channel.sh").read_text())
    builder = fixture / "scripts/build-jcop4-channel-probe.sh"
    builder.write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $# == 2 && -z $1 && $2 == --login-once ]]
printf 'build\n' >> "$FIXTURE_ROOT/calls"
[[ ${FIXTURE_BUILD_FAIL:-0} == 0 ]] || exit 31
cp "$FIXTURE_ROOT/helper" "$FIXTURE_ROOT/build/opensc/jcop4-channel-login"
''')
    helper = fixture / "helper"
    helper.write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $# == 2 && $1 == 0 && $2 == --login-once && $OPENSC_DEBUG == 0 ]]
for var in OPENSC_API_TRACE_FILE OPENSC_DEBUG_FILE LD_PRELOAD LD_AUDIT LD_LIBRARY_PATH LIBPCSCLITE_DELEGATE LIBPCSCLITE_SPY_DELEGATE; do
    [[ ! -v $var ]]
done
if read -r input; then exit 93; fi
[[ $(stat -c %a "$OPENSC_CONF") == 600 ]]
printf 'helper\n' >> "$FIXTURE_ROOT/calls"
printf 'login_result=synthetic_fixture_no_hardware\nfinal_reset_result=0\n'
exit "${FIXTURE_HELPER_EXIT:-0}"
''')
    helper.chmod(0o700)
    (fixture / "bin/git").write_text("#!/usr/bin/env bash\nexit 0\n")
    (fixture / "bin/timeout").write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $1 == --foreground ]] || exit 94
shift
[[ $1 == --signal=TERM && $2 == --kill-after=5s && $3 == 45s ]]
shift 3
exec "$@"
''')
    for path in (fixture / "bin").iterdir():
        path.chmod(0o700)
    env = dict(os.environ, FIXTURE_ROOT=str(fixture), PATH=str(fixture / "bin") + ":" + os.environ["PATH"])
    for key in ("OPENSC_API_TRACE_FILE", "OPENSC_DEBUG_FILE", "LIBPCSCLITE_DELEGATE", "LIBPCSCLITE_SPY_DELEGATE"):
        env[key] = "synthetic_unused"
    for expected in (0, 1, 124):
        env["FIXTURE_HELPER_EXIT"] = str(expected)
        status, output = pty_run(["bash", str(launcher), "0", "--login-once"], tty_stdout=True, environment=env)
        assert status == expected
        report = next(line.removeprefix("Informe: ").strip() for line in output.decode().splitlines() if line.startswith("Informe: "))
        report = Path(report)
        assert report.stat().st_mode & 0o777 == 0o600 and report.parent.stat().st_mode & 0o777 == 0o700
        assert "mode=anonymous_channel_login_once_no_signature" in report.read_text()
        assert f"helper_exit_status={expected}" in report.read_text()
    assert (fixture / "calls").read_text().splitlines() == ["build", "helper"] * 3
    env["FIXTURE_BUILD_FAIL"] = "1"
    status, _ = pty_run(["bash", str(launcher), "0", "--login-once"], tty_stdout=True, environment=env)
    assert status == 31 and (fixture / "calls").read_text().splitlines()[-1] == "build"
    before = (fixture / "calls").read_text()
    status = subprocess.run(["bash", str(launcher), "0", "--login-once"], env=env, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE).returncode
    assert status == 2 and (fixture / "calls").read_text() == before
print("PASS login launcher requires a terminal, selects separate binary, isolates tracing, closes stdin, preserves private reports and never retries failures/timeouts/build errors")
