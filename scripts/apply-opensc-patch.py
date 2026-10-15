#!/usr/bin/env python3
"""Reconstruct the exact patched tree in a clean upstream checkout (CI only)."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
meta = json.loads((root / 'patches/opensc-base.json').read_text())
source = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None
if source is None:
    raise SystemExit('Usage: python3 scripts/apply-opensc-patch.py UPSTREAM_CHECKOUT')
patch = root / 'patches' / meta['patch_file']
def git(*args):
    return subprocess.check_output(['git', '-C', str(source), *args], text=True).strip()
if hashlib.sha256(patch.read_bytes()).hexdigest() != meta['patch_sha256']:
    raise SystemExit('Patch SHA-256 differs from manifest')
if git('rev-parse', 'HEAD') != meta['base_commit'] or git('status', '--porcelain'):
    raise SystemExit('Expected a clean checkout at the exact OpenSC base commit')
subprocess.run(['git', '-C', str(source), 'apply', '--check', str(patch)], check=True)
subprocess.run(['git', '-C', str(source), 'apply', '--index', str(patch)], check=True)
if git('write-tree') != meta['patched_tree']:
    raise SystemExit('Patched tree differs from manifest; do not build or publish it')
print('Patch applied; complete Git tree matches the exported submodule tree')
