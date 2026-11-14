#!/usr/bin/env python3
"""Contracts for Ashfall Era Meds: the replacer plugin and every file the standalone zip ships."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import plugin_reader as pr  # noqa: E402

DEPLOY = ROOT / "deploy"
CONTRACT = json.loads((ROOT / "tools/era_meds.json").read_text(encoding="utf-8"))

failures: list[str] = []


def check(condition, message: str) -> None:
    if not condition:
        failures.append(message)


def zstr(raw) -> str | None:
    return None if raw is None else raw.rstrip(b"\x00").decode("utf-8", "replace")


buf = (DEPLOY / CONTRACT["plugin"]).read_bytes()
nodes = pr.parse(buf)
check(struct.unpack_from("<I", buf, 8)[0] & 0x200, f"{CONTRACT['plugin']} must be ESL flagged")
check(not struct.unpack_from("<I", buf, 8)[0] & 0x80, f"{CONTRACT['plugin']} must not be localized (names are plain text)")
masters = [zstr(bytes(buf[s:s + n])) for sig, s, n in nodes[0].subrecords(buf) if sig == b"MAST"]
check(masters == [CONTRACT["master"]], f"masters must be [{CONTRACT['master']}], got {masters}")

alch = pr.records_by_form_id(buf, nodes, b"ALCH")
check(len(alch) == len(CONTRACT["overrides"]), f"expected {len(CONTRACT['overrides'])} ALCH overrides, found {len(alch)}")
shipped = set()
for entry in CONTRACT["overrides"]:
    node = alch.get(int(entry["recordId"], 16))
    name = entry["editorId"]
    check(node is not None, f"missing override {name} {entry['recordId']}")
    if node is None:
        continue
    check(pr.editor_id(buf, node) == name, f"{entry['recordId']} is not {name}")
    check(zstr(node.field(buf, b"MODL")) == entry["model"], f"{name} must use {entry['model']}")
    check(zstr(node.field(buf, b"FULL")) == entry["name"], f"{name} must be named {entry['name']!r} in plain text")
    check(zstr(node.field(buf, b"DESC")) == entry["desc"], f"{name} description must be plain text")
    check(node.field(buf, b"MODT") is None, f"{name} still carries the vanilla texture hashes")
    shipped.add(entry["model"])

patch = (DEPLOY / "RadOverhaulSurPatch.esp").read_bytes()
patch_alch = pr.records_by_form_id(patch, pr.parse(patch), b"ALCH")
for entry in CONTRACT["overrides"]:
    node = patch_alch.get(int(entry["recordId"], 16))
    if node is not None:
        check(zstr(node.field(patch, b"MODL")) == entry["model"], f"RadOverhaulSurPatch.esp {entry['editorId']} must also use {entry['model']}")

sndr = {pr.editor_id(buf, n): n for n in pr.records_by_form_id(buf, nodes, b"SNDR").values()}
check(sorted(sndr) == sorted(CONTRACT["sounds"]), f"SNDR records must be {CONTRACT['sounds']}, found {sorted(sndr)}")
for name, node in sndr.items():
    wav = f"Sound\\FX\\AshfallEraMeds\\{name}.wav"
    check(zstr(node.field(buf, b"ANAM")) == "data\\" + wav, f"SNDR {name} must play {wav}")
    path = DEPLOY / wav.replace("\\", "/")
    check(path.is_file() and path.read_bytes()[:4] == b"RIFF", f"missing sound {path.relative_to(ROOT)}")

for model in sorted(shipped) + CONTRACT["looseMeshes"]:
    mesh = DEPLOY / "Meshes" / model.replace("\\", "/")
    check(mesh.is_file(), f"missing mesh {mesh.relative_to(ROOT)}")
for folder in CONTRACT["folders"]:
    path = DEPLOY / folder
    check(path.is_dir() and any(path.rglob("*.*")), f"missing or empty {path.relative_to(ROOT)}")

release = ROOT / ".github/workflows/release.yml"
if release.is_file():  # the public CI export ships without workflows
    check("tools/era_meds.json" in release.read_text(encoding="utf-8"), "release.yml must package the Era Meds zip from tools/era_meds.json")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    sys.exit(1)
print(f"Era Meds contract verified: {len(CONTRACT['overrides'])} overrides, {len(shipped) + len(CONTRACT['looseMeshes'])} meshes")
