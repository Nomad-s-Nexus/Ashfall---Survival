#!/usr/bin/env python3
"""Contracts for limb injuries: RadInjury.esp records, the quest script, meshes, DLL ids and HUD hooks."""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import plugin_reader as pr  # noqa: E402

DEPLOY = ROOT / "deploy"
SRC = ROOT / "src"
VIEW = DEPLOY / "PrismaUI_F4/views/Interface/FalloutAnomaly"
CONTRACT = json.loads((ROOT / "tools/rad_limb_injury.json").read_text(encoding="utf-8"))
NAMES = json.loads((ROOT / "tools/rad_injury_names.json").read_text(encoding="utf-8"))["names"]

failures: list[str] = []


def check(condition, message: str) -> None:
    if not condition:
        failures.append(message)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""


def local(value: str) -> int:
    return 0x01000000 | int(value, 16)


def esm(value: str) -> int:
    return int(value, 16)


def zstr(raw) -> str | None:
    return None if raw is None else raw.rstrip(b"\x00").decode("utf-8", "replace")


buf = (DEPLOY / CONTRACT["plugin"]).read_bytes()
nodes = pr.parse(buf)
header = nodes[0]
masters = [zstr(payload) for sig, payload in
           ((sig, bytes(buf[s:s + n])) for sig, s, n in header.subrecords(buf)) if sig == b"MAST"]
check(masters == [CONTRACT["master"]], f"RadInjury.esp masters must be [{CONTRACT['master']}], got {masters}")


def records(signature: bytes) -> dict[int, pr.Node]:
    return pr.records_by_form_id(buf, nodes, signature)


def record(signature: bytes, form_id: str, editor_id: str):
    node = records(signature).get(local(form_id))
    check(node is not None, f"missing {signature.decode()} {editor_id} at {form_id}")
    if node is not None:
        check(pr.editor_id(buf, node) == editor_id, f"{form_id} is not {editor_id}")
    return node


def fields(node, signature: bytes) -> list[bytes]:
    return [bytes(buf[s:s + n]) for sig, s, n in node.subrecords(buf) if sig == signature]


for entry in CONTRACT["globals"]:
    record(b"GLOB", entry["recordId"], entry["editorId"])
global_ids = {entry["editorId"]: local(entry["recordId"]) for entry in CONTRACT["globals"]}


def check_effect_data(node, name: str, actor_value: int, constant: bool) -> None:
    data = node.field(buf, b"DATA")
    check(data is not None and len(data) >= 116, f"{name} has no usable DATA")
    if not data or len(data) < 116:
        return
    check(struct.unpack_from("<I", data, 64)[0] == 34, f"{name} must be a Peak Value Modifier")
    check(struct.unpack_from("<I", data, 68)[0] == actor_value, f"{name} targets the wrong actor value")
    check(struct.unpack_from("<I", data, 0)[0] & 0x800, f"{name} must be area-less")
    check(abs(struct.unpack_from("<f", data, 112)[0] - 1.0) < 1e-6, f"{name} must keep DualCastingScale 1.0")
    casting = struct.unpack_from("<I", data, 80)[0]
    check(casting == (0 if constant else 1), f"{name} has casting type {casting}")


for entry in CONTRACT["effects"]:
    node = record(b"MGEF", entry["recordId"], entry["editorId"])
    if node is None:
        continue
    name = entry["editorId"]
    check_effect_data(node, name, esm(entry["actorValue"]), constant=True)
    conditions = fields(node, b"CTDA")
    check(len(conditions) == 1, f"{name} must carry exactly one condition, found {len(conditions)}")
    if len(conditions) == 1:
        ctda = conditions[0]
        check(ctda[0] >> 5 == 0, f"{name} condition must compare with ==")
        check(abs(struct.unpack_from("<f", ctda, 4)[0] - entry["tier"]) < 1e-6, f"{name} must test tier {entry['tier']}")
        check(struct.unpack_from("<H", ctda, 8)[0] == CONTRACT["conditionFunction"], f"{name} must use GetGlobalValue")
        check(struct.unpack_from("<I", ctda, 12)[0] == global_ids[entry["global"]], f"{name} must read {entry['global']}")

ability = CONTRACT["ability"]
spell = record(b"SPEL", ability["recordId"], ability["editorId"])
if spell is not None:
    spit = spell.field(buf, b"SPIT")
    check(spit is not None and struct.unpack_from("<I", spit, 8)[0] == ability["spellType"], "RadInjury_Ability must be an ability")
    linked = [struct.unpack("<I", raw)[0] for raw in fields(spell, b"EFID")]
    mags = [round(struct.unpack_from("<f", raw, 0)[0], 3) for raw in fields(spell, b"EFIT")]
    expected = sorted((local(e["recordId"]), float(e["magnitude"])) for e in CONTRACT["effects"])
    check(sorted(zip(linked, mags)) == expected, "RadInjury_Ability entries do not match the effect table")

treat = CONTRACT["treatEffect"]
treat_node = record(b"MGEF", treat["recordId"], treat["editorId"])
if treat_node is not None:
    check_effect_data(treat_node, treat["editorId"], esm(treat["actorValue"]), constant=False)

quest = CONTRACT["quest"]
quest_node = record(b"QUST", quest["recordId"], quest["editorId"])
if quest_node is not None:
    script = pr.vmad_properties(buf, quest_node).get(quest["script"], {})
    check(script.get(quest["abilityProperty"]) == local(ability["recordId"]), "the quest script must point Ability at RadInjury_Ability")
    dnam = quest_node.field(buf, b"DNAM")
    check(dnam is not None and dnam[0] & 0x01, "RadInjury_Quest must be start game enabled")

loot = CONTRACT["loot"]
loot_node = record(b"LVLI", loot["recordId"], loot["editorId"])
if loot_node is not None:
    entries = sorted((struct.unpack_from("<I", raw, 4)[0], struct.unpack_from("<H", raw, 8)[0]) for raw in fields(loot_node, b"LVLO"))
    check(entries == sorted((local(f), n) for f, n in loot["entries"]), f"{loot['editorId']} entries {entries}")
if quest_node is not None:
    for name, value in loot["properties"].items():
        wanted = local(value.split(":", 1)[1]) if value.startswith("local:") else esm(value)
        check(script.get(name) == wanted, f"the quest script property {name} must point at {value}")

source = text(DEPLOY / f"Scripts/Source/User/{quest['script']}.psc")
check("AddSpell(Ability" in source and "OnPlayerLoadGame" in source, "the quest script must add the ability on init and load")
check("MedsLoot.AddForm(FirstAidLoot" in source and "VendorMeds.AddForm(FirstAidLoot" in source,
      "the quest script must add the first aid list to the vanilla meds lists")
check((DEPLOY / f"Scripts/{quest['script']}.pex").is_file(), "the compiled quest script is missing")

stimpak_keyword = 0x0F4AEB
for item in CONTRACT["items"]:
    node = record(b"ALCH", item["recordId"], item["editorId"])
    if node is None:
        continue
    name = item["editorId"]
    check(NAMES.get(name) == item["name"], f"{name} display name is not in tools/rad_injury_names.json")
    check(zstr(node.field(buf, b"FULL")) == item["name"], f"{name} must be named {item['name']!r}")
    check(zstr(node.field(buf, b"MODL")) == item["model"], f"{name} must use {item['model']}")
    mesh = DEPLOY / "Meshes" / item["model"].replace("\\", "/")
    check(mesh.is_file(), f"{name} mesh {mesh.relative_to(ROOT)} is missing")
    effects = [struct.unpack("<I", raw)[0] for raw in fields(node, b"EFID")]
    check(effects == [local(treat["recordId"])], f"{name} must carry only RadInjury_TreatFx")
    keywords = node.field(buf, b"KWDA") or b""
    check(stimpak_keyword not in struct.unpack(f"<{len(keywords) // 4}I", keywords), f"{name} must not be a Stimpak")
    check(not fields(node, b"CTDA"), f"{name} still carries Stimpak conditions")
    check(node.field(buf, b"MODT") is None, f"{name} still carries the Stimpak texture hashes")
    data = node.field(buf, b"DATA")
    check(data is not None and abs(struct.unpack_from("<f", data, 0)[0] - item["weight"]) < 1e-6, f"{name} weight")
    enit = node.field(buf, b"ENIT")
    check(enit is not None and struct.unpack_from("<i", enit, 0)[0] == item["value"], f"{name} value")

    recipe = item["recipe"]
    cobj = record(b"COBJ", recipe["recordId"], recipe["editorId"])
    if cobj is None:
        continue
    cnam = cobj.field(buf, b"CNAM")
    check(cnam is not None and struct.unpack("<I", cnam)[0] == local(item["recordId"]), f"{recipe['editorId']} must make {name}")
    bnam = cobj.field(buf, b"BNAM")
    check(bnam is not None and struct.unpack("<I", bnam)[0] == esm(CONTRACT["workbenchKeyword"]), f"{recipe['editorId']} must use the chem station")
    categories = [struct.unpack("<I", raw)[0] for raw in fields(cobj, b"FNAM")]
    check(esm(CONTRACT["recipeCategory"]) in categories, f"{recipe['editorId']} must sit in the chem station category")
    check(not fields(cobj, b"CTDA"), f"{recipe['editorId']} must have no conditions")
    fvpa = cobj.field(buf, b"FVPA") or b""
    pairs = sorted(struct.iter_unpack("<II", fvpa))
    wanted = sorted((esm(c["recordId"]), c["count"]) for c in recipe["components"])
    check(pairs == wanted, f"{recipe['editorId']} components {pairs} != {wanted}")

runtime = text(SRC / "LimbInjuryRuntime.cpp")
first_item = CONTRACT["items"][0]["recordId"]
for token in (f'kPlugin = "{CONTRACT["plugin"]}"',
              f"kLimbGlobalBase = 0x{int(CONTRACT['globals'][0]['recordId'], 16):06X}",
              f"kFxGlobalBase = 0x{int(CONTRACT['globals'][6]['recordId'], 16):06X}",
              f"kItemBase = 0x{int(first_item, 16):06X}",
              f"kStimpakForm = 0x{int(CONTRACT['stimpak'], 16):06X}"):
    check(token in runtime, f"LimbInjuryRuntime.cpp must define {token}")
ids = [int(item["recordId"], 16) for item in CONTRACT["items"]]
check(ids == list(range(ids[0], ids[0] + len(ids))), "item form ids must be contiguous for kItemBase")
check([item["treatKind"] for item in CONTRACT["items"]] == [1, 2, 3, 4, 5], "items must follow TreatKind order")
policy = text(SRC / "LimbInjuryPolicy.h")
check(re.search(r"kTreatNone,\s*kTreatBandage,\s*kTreatSplint,\s*kTreatTourniquet,\s*kTreatSutureKit,\s*kTreatMedKit", policy),
      "TreatKind order must match the item order")
fx = [entry["editorId"] for entry in CONTRACT["globals"][6:]]
check(fx == ["RadInjury_FxLegs", "RadInjury_FxArms", "RadInjury_FxHead", "RadInjury_FxTorso"],
      "Fx globals must follow RegionLevelSet order (legs, arms, head, torso)")

check('a.rfind("limb:", 0) == 0' in text(SRC / "RadEngine_Debug.cpp"), "the debug dispatcher must route limb: commands")
check("renderInjury" in text(VIEW / "hud_needs.html"), "the HUD must render the injury core")
debug_html = text(VIEW / "rad_debug.html")
for token in ("limb:treat:", "limb:injure:", "limb:stim", "limb:clear"):
    check(token in debug_html, f"rad_debug.html must contain {token}")
debug_js = text(VIEW / "rad_debug.js")
for token in ("LimbInjuriesEnabled", "LimbCrippleFractures"):
    check(token in debug_js, f"rad_debug.js must contain {token}")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    sys.exit(1)
print(f"Limb injury contract verified: {len(CONTRACT['effects'])} effects, {len(CONTRACT['items'])} items and recipes")
