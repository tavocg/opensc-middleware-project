"""Launcher fixtures only: fake helper, builder and timeout; no PC/SC."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="jcop4-reader-launcher-") as temporary:
    fixture = Path(temporary)
    (fixture / "scripts").mkdir()
    (fixture / "bin").mkdir()
    (fixture / "build/diagnostics").mkdir(parents=True)
    launcher = fixture / "scripts/probe-jcop4-reader-features.sh"
    shutil.copyfile(root / "scripts/probe-jcop4-reader-features.sh", launcher)
    (fixture / "scripts/build-jcop4-reader-features.sh").write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $# == 0 ]]
printf 'build\n' >> "$FIXTURE_ROOT/calls"
[[ ${FIXTURE_BUILD_FAIL:-0} == 0 ]] || exit 31
cp "$FIXTURE_ROOT/helper" "$FIXTURE_ROOT/build/diagnostics/jcop4-reader-features"
''')
    helper = fixture / "helper"
    helper.write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $# == 1 && $1 == 0 ]]
for var in LD_PRELOAD LD_AUDIT LD_LIBRARY_PATH LIBPCSCLITE_DELEGATE LIBPCSCLITE_SPY_DELEGATE OPENSC_API_TRACE_FILE OPENSC_DEBUG_FILE JCOP4_PUBLIC_DISCOVERY_TRACE JCOP4_PUBLIC_SELECT_SPY_TRACE; do
    [[ ! -v $var ]]
done
if read -r input; then exit 93; fi
printf 'helper\n' >> "$FIXTURE_ROOT/calls"
printf 'reader_features_result=synthetic_no_hardware\n'
exit "${FIXTURE_HELPER_EXIT:-0}"
''')
    helper.chmod(0o700)
    timeout = fixture / "bin/timeout"
    timeout.write_text('''#!/usr/bin/env bash
set -euo pipefail
[[ $1 == --signal=TERM && $2 == --kill-after=5s && $3 == 20s ]]
shift 3
exec "$@"
''')
    timeout.chmod(0o700)
    env = dict(os.environ, FIXTURE_ROOT=str(fixture), PATH=str(fixture / "bin") + ":" + os.environ["PATH"])
    for key in ("OPENSC_API_TRACE_FILE", "OPENSC_DEBUG_FILE", "LIBPCSCLITE_DELEGATE", "LIBPCSCLITE_SPY_DELEGATE"):
        env[key] = "synthetic_unused"
    for status in (0, 1, 124):
        env["FIXTURE_HELPER_EXIT"] = str(status)
        result = subprocess.run(["bash", str(launcher), "0"], env=env, input=b"not_passed_to_helper\n", capture_output=True)
        assert result.returncode == status
        report = Path(next(line.removeprefix("Informe: ") for line in result.stdout.decode().splitlines() if line.startswith("Informe: ")))
        assert report.stat().st_mode & 0o777 == 0o600 and report.parent.stat().st_mode & 0o777 == 0o700
        assert f"helper_exit_status={status}" in report.read_text()
        assert "not_passed_to_helper" not in report.read_text()
    assert (fixture / "calls").read_text().splitlines() == ["build", "helper"] * 3
    print("PASS launcher creates private reports, strips tracing/delegates, closes stdin and preserves0/1/124 with one helper invocation and bounded timeout arguments")
    env["FIXTURE_BUILD_FAIL"] = "1"
    result = subprocess.run(["bash", str(launcher), "0"], env=env, capture_output=True)
    assert result.returncode == 31 and (fixture / "calls").read_text().splitlines()[-1] == "build"
    before = (fixture / "calls").read_text()
    for args in (["-1"], ["x"], ["0", "unexpected"]):
        result = subprocess.run(["bash", str(launcher), *args], env=env, capture_output=True)
        assert result.returncode == 2 and (fixture / "calls").read_text() == before
    print("PASS build failure prevents execution and invalid arguments do not build or invoke the helper")
