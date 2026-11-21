#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path


def split_top(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def declare(type_str, name):
    if "(*)" in type_str:
        return type_str.replace("(*)", f"(*{name})", 1)
    m = re.match(r"^(.*?)(\[\d*\])$", type_str)
    if m:
        return f"{m.group(1)}{name}{m.group(2)}"
    return f"{type_str} {name}"


ast_path, names_path = map(Path, sys.argv[1:])
wanted = [n.strip() for n in names_path.read_text().splitlines() if n.strip() and not n.startswith("#")]

wanted_names = set(wanted)
decls = {}
for node in json.loads(ast_path.read_text())["inner"]:
    if node.get("kind") != "FunctionDecl":
        continue
    name = node.get("name")
    if name not in wanted_names or name in decls:
        continue
    # "hipError_t (void **, size_t)" -- the return type and the parameter list, as clang prints it
    sig = node["type"]["qualType"]
    ret, _, params = sig.partition("(")
    params = params.rstrip()[:-1].strip()
    params = [p.strip() for p in split_top(params)] if params and params != "void" else []
    decls[name] = (ret.strip(), params)

missing = [n for n in wanted if n not in decls]
out = []
for name in wanted:
    if name not in decls:
        continue
    ret, params = decls[name]
    if not params:
        out.append(f"HIP_FN({ret}, {name}, (void), ())")
        continue
    named = []
    for i, p in enumerate(params):
        named.append(declare(p, f"a{i}"))
    args = ", ".join(f"a{i}" for i in range(len(params)))
    out.append(f"HIP_FN({ret}, {name}, ({', '.join(named)}), ({args}))")

print("\n".join(out))
if missing:
    print(f"\n// not declared in the headers, left out: {' '.join(missing)}", file=sys.stderr)
