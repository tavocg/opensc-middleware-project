#!/usr/bin/env bash
# Package the already tested Linux build. Never initializes PKCS11 or PC/SC.
set -euo pipefail
umask 022
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_source="$task_root/vendor/opensc"
task_build="$task_root/build/opensc"
task_output="$task_root/build/release"
[[ $# == 0 ]] || { printf 'Usage: bash scripts/package-release.sh\n' >&2; exit 2; }
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || {
    printf 'Release packaging currently supports Linux x86_64 only.\n' >&2; exit 2;
}
for task_command in patchelf nix-store pkg-config python3 tar xz sha256sum; do
    command -v "$task_command" >/dev/null || { printf 'Missing %s; enter nix develop.\n' "$task_command" >&2; exit 1; }
done
[[ -f "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" && -f "$task_build/src/libopensc/.libs/libopensc.so" ]] || {
    printf 'Compile and test OpenSC before packaging.\n' >&2; exit 1;
}
# Refuse stale exports or a source tree different from the advertised patch.
python3 - "$task_root" <<'PY'
import hashlib, json, subprocess, sys
from pathlib import Path
root = Path(sys.argv[1]); meta = json.loads((root/'patches/opensc-base.json').read_text())
patch = (root/'patches'/meta['patch_file']).read_bytes()
current = subprocess.check_output(['git','-C',str(root/'vendor/opensc'),'diff','--binary','--full-index','--no-ext-diff','--no-color','--no-textconv','--no-renames',meta['base_commit'],'--'])
if patch != current or hashlib.sha256(patch).hexdigest() != meta['patch_sha256']:
    raise SystemExit('Exported patch/manifest does not match the current OpenSC source')
PY
# A failed or repeated package operation never mixes previous output into a release.
if [[ -e "$task_output" ]]; then
    printf 'build/release already exists; move it aside before packaging again.\n' >&2; exit 1
fi
mkdir -p -- "$task_output"
task_stage=$(mktemp -d "$task_root/build/release-stage-XXXXXXXX")
trap 'rm -rf -- "$task_stage"' EXIT
task_bundle="$task_stage/libjcop3p11-linux-x86_64"
mkdir -p -- "$task_bundle/lib" "$task_bundle/config" "$task_bundle/bin"
cp -- "$task_build/src/pkcs11/.libs/opensc-pkcs11.so" "$task_bundle/lib/libjcop3p11.so"
task_soname=$(patchelf --print-soname "$task_build/src/libopensc/.libs/libopensc.so")
[[ $task_soname =~ ^libopensc\.so\.[0-9]+$ ]] || { printf 'Unexpected libopensc SONAME.\n' >&2; exit 1; }
cp -- "$task_build/src/libopensc/.libs/libopensc.so" "$task_bundle/lib/$task_soname"
chmod 755 "$task_bundle/lib/libjcop3p11.so" "$task_bundle/lib/$task_soname"
# Keep only Nix runtime entries and sibling libraries, never checkout/build paths.
python3 - "$task_bundle/lib" "$task_stage/runtime-roots.txt" <<'PY'
import re, subprocess, sys
from pathlib import Path
roots = set(); directory = Path(sys.argv[1])
for library in directory.iterdir():
    original = subprocess.check_output(['patchelf','--print-rpath',str(library)],text=True).strip().split(':')
    runtime = [p for p in original if re.fullmatch(r'/nix/store/[a-z0-9]{32}-[^/]+/lib(?:/[^:]*)?',p)]
    if not runtime: raise SystemExit('Expected Nix-linked release libraries')
    for p in runtime: roots.add('/'.join(p.split('/')[:4]))
    subprocess.run(['patchelf','--set-rpath',':'.join(['$ORIGIN', *dict.fromkeys(runtime)]),str(library)],check=True)
    if subprocess.check_output(['patchelf','--print-rpath',str(library)],text=True).strip() != ':'.join(['$ORIGIN', *dict.fromkeys(runtime)]):
        raise SystemExit('RPATH rewrite failed')
# The PCSC provider is opened dynamically and therefore is not an ELF NEEDED.
pcsc = subprocess.check_output(['pkg-config','--variable=libdir','libpcsclite'],text=True).strip()
match = re.match(r'(/nix/store/[a-z0-9]{32}-[^/]+)/lib$',pcsc)
if not match: raise SystemExit('Expected the locked Nix PCSC runtime')
roots.add(match[1])
Path(sys.argv[2]).write_text(''.join(p+'\n' for p in sorted(roots)))
PY
# Loading and querying the vtable does not initialize Cryptoki or discover readers.
env -u LD_PRELOAD -u LD_AUDIT -u LD_LIBRARY_PATH python3 - "$task_bundle/lib/libjcop3p11.so" <<'PY'
import ctypes, sys
module = ctypes.CDLL(sys.argv[1])
function = module.C_GetFunctionList
function.argtypes = [ctypes.POINTER(ctypes.c_void_p)]; function.restype = ctypes.c_ulong
pointer = ctypes.c_void_p()
if function(ctypes.byref(pointer)) != 0 or not pointer.value:
    raise SystemExit('Packaged PKCS11 module did not provide its function table')
print('Packaged module loaded and exposed C_GetFunctionList; no C_Initialize')
PY
cp -- "$task_root/scripts/run-release-firmador.sh" "$task_bundle/bin/firmador-jcop"
chmod 755 "$task_bundle/bin/firmador-jcop"
cp -- "$task_root/config/jcop3.conf" "$task_root/config/jcop4.conf" "$task_bundle/config/"
cp -- "$task_root/INSTALL.md" "$task_source/COPYING" "$task_bundle/"
cp -- "$task_root/patches/opensc-base.json" "$task_bundle/opensc-base.json"
cp -- "$task_bundle/lib/libjcop3p11.so" "$task_bundle/lib/$task_soname" "$task_output/"
cp -- "$task_root/patches/opensc-jcop3-jcop4.diff" "$task_root/patches/opensc-base.json" "$task_root/INSTALL.md" "$task_output/"
readarray -t task_runtime_roots < "$task_stage/runtime-roots.txt"
nix-store --query --requisites "${task_runtime_roots[@]}" | LC_ALL=C sort -u > "$task_stage/runtime-paths.txt"
readarray -t task_runtime_paths < "$task_stage/runtime-paths.txt"
nix-store --export "${task_runtime_paths[@]}" | xz -T2 > "$task_output/nix-runtime-linux-x86_64.nar.xz"
cp -- "$task_stage/runtime-paths.txt" "$task_output/nix-runtime-paths.txt"
# The target tree exists locally or was created by the CI patch verification.
# git archive reads it without needing the unpublished target commit.
task_patched_tree=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["patched_tree"])' "$task_root/patches/opensc-base.json")
git -C "$task_source" archive --format=tar --prefix=opensc/ "$task_patched_tree" | \
    xz -T2 > "$task_output/opensc-patched-source.tar.xz"
XZ_OPT=-T2 tar -C "$task_stage" -cJf "$task_output/libjcop3p11-linux-x86_64.tar.xz" libjcop3p11-linux-x86_64
python3 - "$task_root" "$task_output" <<'PY'
import hashlib, json, os, subprocess, sys
from pathlib import Path
root, out = map(Path,sys.argv[1:])
manifest = json.loads((root/'patches/opensc-base.json').read_text())
manifest.update(system='x86_64-linux', project_commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),
                github_ref=os.environ.get('GITHUB_REF','local-unpublished'), runtime='Nix store closure; compatible host/application glibc required',
                asset_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.iterdir()) if p.is_file()})
(out/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
PY
cat > "$task_output/RELEASE-NOTES.md" <<'NOTES'
Experimental libjcop3p11 build for GNU/Linux x86_64 using the locked Nix environment.

Load libjcop3p11.so from the extracted bundle; it also needs the sibling libopensc
library and the Nix runtime. This is not a standalone or universally portable .so.
See INSTALL.md for importing the runtime and launching firmador for JCOP3/JCOP4.
The application/host glibc must be compatible with the build; importing Nix paths
alone does not upgrade the libc of an already running non-Nix application.

JCOP4 is opt-in and limited to the observed Costa Rican RSA2048/SHA256 profile;
RSA3072 and other card personalizations are not validated. CI uses simulated
readers/credentials and never validates a real card or handles its PIN.

Assets include the exact upstream base, patch, corresponding OpenSC source,
both libraries, runtime closure, build manifest and SHA-256 checksums.
NOTES
(cd -- "$task_output" && sha256sum -- ./* > "$task_stage/SHA256SUMS")
cp -- "$task_stage/SHA256SUMS" "$task_output/SHA256SUMS"
printf 'Release artifacts prepared in %s; nothing published.\n' "$task_output"
