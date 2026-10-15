#!/usr/bin/env python3
"""Export a pinned OpenSC patch and its JCOP3/JCOP4 change map (no hardware)."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "vendor/opensc"
META = ROOT / "patches/opensc-base.json"
PATCH = ROOT / "patches/opensc-jcop3-jcop4.diff"


def git(*args):
    return subprocess.check_output(["git", "-C", str(SOURCE), *args])


def clean_c(text):
    # Preserve positions/newlines while hiding comments, literals and macros.
    pattern = r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    text = re.sub(pattern, lambda m: re.sub(r"[^\n]", " ", m[0]), text)
    return re.sub(r"(?m)^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", lambda m: re.sub(r"[^\n]", " ", m[0]), text)


def functions(text):
    """Top-level definitions, including alternate preprocessor implementations."""
    cleaned = clean_c(text)
    result = {}
    depth, start, name = 0, 0, None
    for pos, char in enumerate(cleaned):
        if char == "{":
            if depth == 0:
                end = pos - 1
                while end >= 0 and cleaned[end].isspace():
                    end -= 1
                if end >= 0 and cleaned[end] == ")":
                    parens, begin = 1, end - 1
                    while begin >= 0 and parens:
                        parens += (cleaned[begin] == ")") - (cleaned[begin] == "(")
                        begin -= 1
                    match = re.search(r"([A-Za-z_]\w*)\s*$", cleaned[:begin + 1])
                    if match and match[1] not in {"if", "while", "for", "switch", "sizeof"}:
                        name, start = match[1], match.start(1)
                    else:
                        name = None
                else:
                    name = None
                body = pos + 1
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0 and name:
                item = result.setdefault(name, {"ranges": [], "bodies": []})
                item["ranges"].append((text.count("\n", 0, start) + 1, text.count("\n", 0, pos) + 1))
                item["bodies"].append(cleaned[body:pos])
                name = None
    return result


def file_scopes(path):
    name = Path(path).name
    if name.startswith(("jcop4-", "iasecc-chipdoc")) or name in {"cards.h", "opensc.conf.example.in", "pkcs15.c", "misc.c"}:
        return [4]
    if name in {"pkcs15-iasecc.c", "pkcs15-pin.c", "iasecc-sdo.c", "iasecc-sdo.h", "opensc.h", "card.c", "iso7816.c", "pkcs11-tool.c"}:
        return [3]
    return [3, 4]


def function_scopes(path, name):
    if Path(path).name == "card-iasecc.c":
        return [4] if name in {
            "iasecc_match_card", "iasecc_init", "iasecc_logout", "iasecc_finish", "sc_get_driver",
            "iasecc_chipdoc_invalidate_card", "iasecc_card_reader_lock_obtained",
        } else [3]
    if Path(path).name == "framework-pkcs15.c":
        if name == "pkcs15_chipdoc_session_state":
            return [3]
        if name in {"C_GetTokenInfo", "slot_get_logged_in_state", "pkcs15_prkey_sign"}:
            return [3, 4]
        return [4]
    return file_scopes(path)


def render(files, base, target):
    incoming = {(e["file"], e["function"]) for f in files for fn in f["functions"] for e in fn["edges"]}
    lines = ["flowchart TB", "  %% Solid: membership/direct call. Dotted: callback/reference."]
    lookup = {}
    for scope in (3, 4):
        lines += [f'  subgraph scope{scope}["JCOP{scope}"]', "    direction TB", f'    JCOP{scope}["JCOP{scope}"]:::root']
        for index, item in enumerate(files):
            selected = [f for f in item["functions"] if scope in f["scopes"]]
            if scope not in item["scopes"] and not selected:
                continue
            file_id = f"c{scope}_file{index}"
            state = "AGREGADO" if item["added_file"] else "EXISTENTE"
            shared = " · compartido" if len(item["scopes"]) == 2 else ""
            lines += [f'    subgraph {file_id}_group["{Path(item["path"]).parent}"]',
                      f'      {file_id}["{Path(item["path"]).name}<br/>{state}{shared}<br/>+{item["added"]} / -{item["deleted"]}"]:::file',
                      f'      JCOP{scope} --> {file_id}']
            for fn_index, fn in enumerate(selected):
                node = f"c{scope}_fn{index}_{fn_index}"
                lookup[scope, item["path"], fn["name"]] = node
                kind = "NUEVA" if fn["new"] else "MODIFICADA"
                css = "added" if fn["new"] else "modified"
                lines.append(f'      {node}["{fn["name"]}()<br/>{kind}"]:::{css}')
                if not fn["new"] or (item["path"], fn["name"]) not in incoming:
                    lines.append(f"      {file_id} --> {node}")
            lines.append("    end")
        lines.append("  end")
    edges = set()
    for item in files:
        for fn in item["functions"]:
            for scope in fn["scopes"]:
                source = lookup[scope, item["path"], fn["name"]]
                for edge in fn["edges"]:
                    dest = lookup.get((scope, edge["file"], edge["function"]))
                    if not dest:
                        other = 4 if scope == 3 else 3
                        dest = lookup[other, edge["file"], edge["function"]]
                    arrow = "-->" if edge["kind"] == "call" else "-. callback / referencia .->"
                    edges.add(f"  {source} {arrow} {dest}")
    lines += sorted(edges)
    lines += ["  classDef root fill:#183153,color:#fff,stroke:#183153", "  classDef file fill:#edf2f7,color:#111,stroke:#64748b", "  classDef added fill:#dcfce7,color:#111,stroke:#15803d", "  classDef modified fill:#ffedd5,color:#111,stroke:#c2410c", ""]
    diagram = "\n".join(lines)
    intro = f'''# Superficie modificada de OpenSC

Base master: `{base}`. Submódulo: `{target}`.

Verde = función **nueva respecto a master**; naranja = función existente
modificada. Las flechas de llamadas solo terminan en funciones nuevas: no se
reproduce el grafo de llamadas ya existente en master. Las flechas desde
JCOP3/JCOP4 y desde archivos expresan pertenencia, no llamadas. Punteadas:
referencias/callbacks identificables por nombre, no prueba de ejecución.

La separación sigue las rutas Generic PKI/AMOS (JCOP3) y el perfil optativo
CR RSA2048 (JCOP4); no implica soporte para todas las tarjetas de esas familias.
Archivos compartidos se muestran bajo ambas raíces, pero sus cifras se cuentan
una sola vez. Cada + / - corresponde al **archivo completo**, no a cada perfil.
Headers de declaraciones, datos, configuración y build pueden no tener funciones.
Se incluyen helpers internos y wrappers aún sin llamador en este submódulo;
una función sin flecha entrante no acredita un recorrido activo.

El análisis es léxico: reconoce llamadas directas y referencias explícitas;
no resuelve punteros de función genéricos, macros ni ejecución condicional.
Las implementaciones alternas de una función por preprocesador se agrupan.

El [diagrama completo está en README.md](../README.md#superficie-modificada-de-opensc).

## Inventario por archivo

| Archivo | Estado | + | - | Ruta | Funciones nuevas (N) y modificadas (M) |
| --- | --- | ---: | ---: | --- | --- |
'''
    for item in files:
        names = "<br/>".join(f'`{fn["name"]}` ({"N" if fn["new"] else "M"})' for fn in item["functions"]) or "Datos / declaraciones / configuración / build"
        intro += f'| `{item["path"]}` | {"Agregado" if item["added_file"] else "Existente"} | {item["added"]} | {item["deleted"]} | {", ".join("JCOP" + str(s) for s in item["scopes"])} | {names} |\n'
    intro += "\nGenerar de nuevo: `python3 scripts/export-opensc-patch.py`. Verificar sin escribir: añadir `--check`.\n"
    return diagram, intro


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", help="Upstream commit; defaults to the saved base, or origin/master")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if git("status", "--porcelain").strip():
        raise SystemExit("Commit the OpenSC changes before exporting a pinned patch.")
    saved = json.loads(META.read_text()) if META.exists() else {}
    base = git("rev-parse", args.base or saved.get("base_commit", "origin/master")).decode().strip()
    target = git("rev-parse", "HEAD").decode().strip()
    patch = git("diff", "--binary", "--full-index", "--no-ext-diff", "--no-color", "--no-textconv", "--no-renames", base, target, "--")
    files = []
    for row in git("diff", "--numstat", "--no-renames", base, target, "--").decode().splitlines():
        added, deleted, path = row.split("\t")
        exists = subprocess.run(["git", "-C", str(SOURCE), "cat-file", "-e", f"{base}:{path}"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
        old = git("show", f"{base}:{path}").decode() if exists else ""
        current = git("show", f"{target}:{path}").decode()
        original, candidates = functions(old), functions(current)
        hunks = git("diff", "--unified=0", base, target, "--", path).decode()
        changed = [(int(a), int(a) + max(int(b or "1"), 1) - 1) for a, b in re.findall(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", hunks, re.M)]
        affected = []
        if path.endswith((".c", ".h")):
            for name, fn in candidates.items():
                new = name not in original
                if new or any(a <= end and b >= begin for begin, end in fn["ranges"] for a, b in changed):
                    affected.append({"name": name, "new": new, "scopes": function_scopes(path, name), "body": "\n".join(fn["bodies"])})
        files.append({"path": path, "added": int(added), "deleted": int(deleted), "added_file": not exists, "scopes": file_scopes(path), "functions": affected})
    new_functions = [(item["path"], fn["name"]) for item in files for fn in item["functions"] if fn["new"]]
    for item in files:
        for fn in item["functions"]:
            fn["edges"] = []
            references = re.sub(r"\b(?:struct|union|enum)\s+[A-Za-z_]\w*", "", fn["body"])
            for path, name in new_functions:
                if re.search(r"\b" + re.escape(name) + r"\b", references):
                    kind = "call" if re.search(r"\b" + re.escape(name) + r"\s*\(", references) else "reference"
                    fn["edges"].append({"file": path, "function": name, "kind": kind})
            del fn["body"]
    # All newly defined names must really be absent from the changed master sources.
    diagram, markdown = render(files, base, target)
    metadata = {"schema": 1, "upstream": "https://github.com/OpenSC/OpenSC.git", "base_commit": base,
                "patched_commit": target, "patched_tree": git("rev-parse", "HEAD^{tree}").decode().strip(),
                "patch_file": PATCH.name, "patch_sha256": hashlib.sha256(patch).hexdigest(),
                "files": len(files), "added_lines": sum(f["added"] for f in files), "deleted_lines": sum(f["deleted"] for f in files)}
    outputs = {PATCH: patch, META: (json.dumps(metadata, indent=2) + "\n").encode(),
               ROOT / "docs/opensc-surface.mmd": diagram.encode(), ROOT / "docs/opensc-surface.md": markdown.encode(),
               ROOT / "docs/opensc-surface.json": (json.dumps({"base": base, "target": target, "files": files}, indent=2) + "\n").encode()}
    readme_path = ROOT / "README.md"
    readme = readme_path.read_text()
    begin, end = "<!-- BEGIN OPENSC SURFACE -->", "<!-- END OPENSC SURFACE -->"
    section = f'''{begin}
## Superficie modificada de OpenSC

Base `{base[:9]}`; submódulo `{target[:9]}`. Verde: función nueva; naranja:
función existente modificada. Los archivos indican **+añadidas / -eliminadas**
frente a la base. Archivos compartidos se repiten visualmente en ambas rutas,
pero las cifras no se suman dos veces. Las flechas entre funciones terminan
solo en funciones nuevas. Punteadas: callback o referencia explícita.
Cada subgrupo contiene un archivo y sus funciones; las flechas desde el archivo
marcan funciones modificadas o entradas sin otro llamador nuevo identificado.

<details>
<summary>Ver diagrama JCOP3 / JCOP4 completo</summary>

```mermaid
{diagram}```

</details>

[Inventario completo y límites del análisis](docs/opensc-surface.md).
[Mermaid sin formato Markdown](docs/opensc-surface.mmd).
[Aplicar el parche e instalar la release](INSTALL.md).
{end}'''
    if begin in readme:
        a = readme.index(begin)
        b = readme.index(end, a) + len(end)
        readme = readme[:a] + section + readme[b:]
    else:
        readme = readme.rstrip() + "\n\n" + section + "\n"
    outputs[readme_path] = readme.encode()
    for path, content in outputs.items():
        if args.check:
            if not path.exists() or path.read_bytes() != content:
                raise SystemExit(f"Stale export: {path.relative_to(ROOT)}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
    print(f'{"Checked" if args.check else "Exported"}: {len(files)} files, {len(new_functions)} new functions; {base[:9]} -> {target[:9]}')


if __name__ == "__main__":
    main()
