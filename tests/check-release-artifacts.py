#!/usr/bin/env python3
"""Check a built release without C_Initialize, readers, PINs, GUI or network."""
import ctypes
import hashlib
import io
import json
import re
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
out=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else None
if out is None:
    raise SystemExit('Usage: python3 tests/check-release-artifacts.py build/release')
subprocess.run(['sha256sum','--strict','--check','SHA256SUMS'],cwd=out,stdout=subprocess.DEVNULL,check=True)
meta=json.loads((ROOT/'patches/opensc-base.json').read_text())
manifest=json.loads((out/'build-manifest.json').read_text())
assert all(manifest[k]==meta[k] for k in meta)
for name,digest in manifest['asset_sha256'].items():
    assert hashlib.sha256((out/name).read_bytes()).hexdigest()==digest,name

def snapshot(archive,prefix):
    result={}
    for item in archive:
        if item.isdir():
            continue
        assert item.name.startswith(prefix),item.name
        name=item.name[len(prefix):]
        if item.isfile():
            result[name]=(item.mode,hashlib.sha256(archive.extractfile(item).read()).hexdigest())
        elif item.issym():
            result[name]=('link',item.linkname)
        elif not item.isdir():
            raise AssertionError('Unexpected source archive entry: '+item.name)
    return result

expected=subprocess.check_output(['git','-C',str(ROOT/'vendor/opensc'),'archive',meta['patched_tree']])
with tarfile.open(fileobj=io.BytesIO(expected)) as a,tarfile.open(out/'opensc-patched-source.tar.xz') as b:
    assert snapshot(a,'')==snapshot(b,'opensc/')
with tempfile.TemporaryDirectory(prefix='libjcop3p11-package-') as temporary:
    with tarfile.open(out/'libjcop3p11-linux-x86_64.tar.xz') as archive:
        archive.extractall(temporary,filter='data')
    bundle=Path(temporary)/'libjcop3p11-linux-x86_64'
    assert (bundle/'bin/firmador-jcop').stat().st_mode & 0o111
    assert (bundle/'config/jcop3.conf').read_bytes()==(ROOT/'config/jcop3.conf').read_bytes()
    assert (bundle/'config/jcop4.conf').read_bytes()==(ROOT/'config/jcop4.conf').read_bytes()
    libraries=list((bundle/'lib').iterdir())
    assert len(libraries)==2
    for library in libraries:
        assert library.read_bytes()==(out/library.name).read_bytes()
        rpath=subprocess.check_output(['patchelf','--print-rpath',str(library)],text=True).strip().split(':')
        assert rpath[0]=='$ORIGIN'
        assert all(re.fullmatch(r'/nix/store/[a-z0-9]{32}-[^/]+/lib(?:/[^:]*)?',p) for p in rpath[1:])
    module=ctypes.CDLL(str(bundle/'lib/libjcop3p11.so'))
    fn=module.C_GetFunctionList
    fn.argtypes=[ctypes.POINTER(ctypes.c_void_p)];fn.restype=ctypes.c_ulong
    pointer=ctypes.c_void_p()
    assert fn(ctypes.byref(pointer))==0 and pointer.value
    class DlInfo(ctypes.Structure):
        _fields_=[('filename',ctypes.c_char_p),('base',ctypes.c_void_p),('symbol',ctypes.c_char_p),('address',ctypes.c_void_p)]
    dladdr=ctypes.CDLL(None).dladdr
    dladdr.argtypes=[ctypes.c_void_p,ctypes.POINTER(DlInfo)];dladdr.restype=ctypes.c_int
    info=DlInfo()
    assert dladdr(ctypes.cast(module.sc_get_version,ctypes.c_void_p),ctypes.byref(info))
    assert Path(info.filename.decode()).resolve().parent==(bundle/'lib').resolve()
paths=set((out/'nix-runtime-paths.txt').read_text().splitlines())
for path in paths:
    references=set(subprocess.check_output(['nix-store','--query','--references',path],text=True).splitlines())
    assert references<=paths,path
subprocess.run(['xz','--test',str(out/'nix-runtime-linux-x86_64.nar.xz')],check=True)
print('PASS release checksums/manifest, complete source tree, bundle/config/permissions, relative RPATH, sibling libopensc, PKCS11 function table and closed Nix runtime')
