#!/usr/bin/env python3
"""Contract for RadOutbreak.esp: records, names, VMAD bindings, sounds, tuning keys and the sick-player cough, read from tools/rad_outbreak.json."""

from __future__ import annotations

import json
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "deploy/RadOutbreak.esp"
CONTRACT = ROOT / "tools/rad_outbreak.json"
NAMES = ROOT / "tools/rad_outbreak_names.json"
SOURCES = ROOT / "deploy/Scripts/Source/User"
PATCH_HINT = ("run python tools/patch_full_names.py --plugin deploy/RadOutbreak.esp "
              "--manifest tools/rad_outbreak_names.json (an editor save drops the patched names)")

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plugin_reader  # noqa: E402

failures: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.is_file() else ""


def zstr(raw: bytes | None) -> str | None:
    return None if raw is None else raw.rstrip(b"\x00").decode("utf-8", "replace")


def u32(raw: bytes | None, offset: int = 0) -> int | None:
    return struct.unpack_from("<I", raw, offset)[0] if raw is not None and len(raw) >= offset + 4 else None


def same(a: object, b: object) -> bool:
    if isinstance(a, bool) or isinstance(b, bool):
        return a is b
    if isinstance(a, float) or isinstance(b, float):
        return isinstance(a, (int, float)) and isinstance(b, (int, float)) and math.isclose(a, b, rel_tol=1e-5, abs_tol=1e-6)
    return a == b


check(PLUGIN.is_file(), f"missing {PLUGIN.relative_to(ROOT)}")
check(CONTRACT.is_file(), f"missing {CONTRACT.relative_to(ROOT)}")
check(NAMES.is_file(), f"missing {NAMES.relative_to(ROOT)}")
contract = json.loads(text(CONTRACT)) if CONTRACT.is_file() else {}
manifest = json.loads(text(NAMES)) if NAMES.is_file() else {}

esp = PLUGIN.read_bytes() if PLUGIN.is_file() else b""
tree: list = []
if esp:
    check(esp[:4] == b"TES4", "RadOutbreak.esp is not a plugin")
    try:
        tree = plugin_reader.parse(esp)
    except plugin_reader.PluginError as exc:
        check(False, f"RadOutbreak.esp does not parse as a plugin: {exc}")

nodes = {node.form_id(esp): node for node in plugin_reader.walk(tree) if node.kind == "record" and node.sig != b"TES4"}
header = next((node for node in tree if node.kind == "record" and node.sig == b"TES4"), None)
masters = [zstr(esp[start:start + size]) for sig, start, size in header.subrecords(esp) if sig == b"MAST"] if header else []


def form(ref: object) -> int | None:
    local, _, plugin = str(ref).split()[0].partition(":")
    value = int(local, 16) & 0x00FFFFFF
    if plugin and plugin != contract.get("plugin"):
        return (masters.index(plugin) << 24 | value) if plugin in masters else None
    return len(masters) << 24 | value


def node_of(ref: object):
    key = form(ref)
    return nodes.get(key) if key is not None else None


def edid(node) -> str | None:
    return plugin_reader.editor_id(esp, node) if node is not None else None


def sig_of(node) -> str:
    return node.sig.decode("ascii") if node is not None else "absent"


def field(node, signature: bytes) -> bytes | None:
    return node.field(esp, signature) if node is not None else None


def scripts_of(node) -> dict:
    try:
        return plugin_reader.vmad_properties(esp, node)
    except (plugin_reader.PluginError, struct.error) as exc:
        check(False, f"{edid(node)} VMAD does not decode: {exc}")
        return {}


check(masters == contract.get("masters"), f"RadOutbreak.esp masters {masters} must equal the contract {contract.get('masters')}")
readme = text(ROOT / "README.md")
check(re.search(r"RadOutbreak\.esp\s+optional NPC and companion disease \(needs RadSurvival\.esp\)", readme) is not None,
      "the README must say RadOutbreak.esp needs RadSurvival.esp")

for rec in contract.get("records", []):
    node = node_of(rec["formKey"])
    label = f"{rec['type']} {rec['localId']} {rec['editorId']}"
    check(node is not None and sig_of(node) == rec["type"] and edid(node) == rec["editorId"],
          f"missing or wrong record {label}: found {sig_of(node)} {edid(node)}")
    if node is None or sig_of(node) != rec["type"]:
        continue
    kind = rec["type"]
    if kind == "MGEF" and "archetype" in rec:
        data = field(node, b"DATA") or b""
        check(u32(data, 0) == rec["flags"] and u32(data, 64) == rec["archetype"] and u32(data, 68) == form(rec["actorValue"]),
              f"{label} must keep flags {rec['flags']}, archetype {rec['archetype']} and actor value {rec['actorValue']}")
    elif kind == "SPEL" and "effect" in rec:
        efit = field(node, b"EFIT") or b""
        check(u32(field(node, b"EFID")) == form(rec["effect"]), f"{label} must carry effect {rec['effect']}")
        if "magnitude" in rec:
            check(len(efit) >= 4 and same(struct.unpack_from("<f", efit)[0], float(rec["magnitude"])),
                  f"{label} effect magnitude must be {rec['magnitude']}")
        if "spellType" in rec:
            check(u32(field(node, b"SPIT"), 8) == rec["spellType"], f"{label} must be spell type {rec['spellTypeName']}")
    elif kind == "QUST":
        check(u32(field(node, b"DNAM")) is not None and (u32(field(node, b"DNAM")) & 0xFFFF) == rec["flags"],
              f"{label} must keep quest flags {rec['flags']} ({rec['flagsMeaning']})")
        check(rec["script"] in scripts_of(node), f"{label} must attach {rec['script']}")
    elif kind == "FURN":
        check(rec["script"] in scripts_of(node), f"{label} must carry the {rec['script']} script")
    elif kind == "FLST":
        actual = sorted(u32(esp[start:start + size]) for sig, start, size in node.subrecords(esp) if sig == b"LNAM")
        check(actual == sorted(form(item) for item in rec["items"]), f"{label} entries diverged from the contract")
    elif kind == "COBJ":
        pairs = field(node, b"FVPA") or b""
        actual = [struct.unpack_from("<II", pairs, i) for i in range(0, len(pairs) - 7, 8)]
        check(u32(field(node, b"CNAM")) == form(rec["createdObject"]), f"{label} must create {rec['createdObject']}")
        check(u32(field(node, b"BNAM")) == form(rec["workbench"]), f"{label} must use workbench {rec['workbench']}")
        check(u32(field(node, b"FNAM")) == form(rec["category"]), f"{label} must use category {rec['category']}")
        check(actual == [(form(c["form"]), c["count"]) for c in rec["components"]], f"{label} components diverged from the contract")
    elif kind == "SNDR":
        files = [zstr(esp[start:start + size]).lower() for sig, start, size in node.subrecords(esp) if sig == b"ANAM"]
        check(files == [path.lower() for path in rec["files"]], f"{label} sound files diverged from the contract")

voice = contract.get("coughVoice", {}).get("sndrMatch", {})
for descriptor in contract.get("sounds", {}).get("descriptors", []):
    node = node_of(descriptor["record"])
    if node is None:
        continue
    bnam = field(node, b"BNAM") or b""
    attenuation = struct.unpack_from("<H", bnam, 4)[0] if len(bnam) >= 6 else None
    expected = voice.get("staticAttenuation" + descriptor["sex"].capitalize())
    check(u32(field(node, b"GNAM")) == form(voice.get("category", "0:Fallout4.esm")) if voice else False,
          f"{edid(node)} category must be the vanilla {voice.get('category')}")
    check(u32(field(node, b"ONAM")) == form(voice.get("outputModel", "0:Fallout4.esm")) if voice else False,
          f"{edid(node)} output model must be the vanilla {voice.get('outputModel')}")
    check(attenuation == expected, f"{edid(node)} static attenuation is {attenuation}, the contract wants {expected}")

by_edid = {edid(node): node for node in nodes.values()}
fails_name = f"; {PATCH_HINT}"
for name, display in manifest.get("names", {}).items():
    node = by_edid.get(name)
    check(zstr(field(node, b"FULL")) == display, f"{name} FULL is {zstr(field(node, b'FULL'))!r}, expected {display!r}{fails_name}")
for name, description in manifest.get("descriptions", {}).items():
    node = by_edid.get(name)
    check(zstr(field(node, b"DESC" if sig_of(node) == "SPEL" else b"DNAM")) == description,
          f"{name} description is not the manifest text{fails_name}")
check(contract.get("scanner", {}).get("displayName") == manifest.get("names", {}).get("RadOutbreak_Scanner") == "Pathogen Scanner",
      "the contract and the names manifest must both name the scanner Pathogen Scanner")
check(contract.get("names", {}).get("RadOutbreak_DoctorStation") == manifest.get("names", {}).get("RadOutbreak_DoctorStation"),
      "the contract and the names manifest must agree on the Doctor Station name")
pipboy = contract.get("pipboy", {})
for stage, display in pipboy.get("names", {}).items():
    for ref in (pipboy["mgefs"][stage], pipboy["spells"][stage]):
        node = node_of(ref)
        check(zstr(field(node, b"FULL")) == display and manifest.get("names", {}).get(edid(node)) == display,
              f"Pip-Boy {ref} must be named {display!r} in the plugin and the manifest{fails_name}")


def check_script(owner: str, section: dict, script: str) -> None:
    node = node_of(section["record"]) if "record" in section else None
    bound = scripts_of(node).get(script)
    check(bound is not None, f"{owner} must attach {script}")
    if bound is None:
        return
    source = text(SOURCES / f"{script}.psc")
    for name, ref in section.get("objectProperties", {}).items():
        check(bound.get(name) == form(ref), f"{script}.{name} must bind {ref}, found {bound.get(name)}")
    for group, pattern in (("floatProperties", "float"), ("intProperties", "int"), ("boolProperties", "bool")):
        for name, value in section.get(group, {}).items():
            if name in bound:
                check(same(bound[name], value), f"{script}.{name} is {bound[name]!r}, the contract wants {value!r}")
                continue
            match = re.search(rf"(?im)^\s*{pattern}\s+Property\s+{name}\s*=\s*(\S+)\s+Auto\b", source)
            default = None if match is None else (match.group(1).lower() == "true" if pattern == "bool" else float(match.group(1)))
            check(match is not None and same(default, value),
                  f"{script}.{name} is neither bound in the VMAD nor defaulted to {value!r} in {script}.psc")
    for name, value in section.get("constProperties", {}).items():
        check(re.search(rf"(?im)^\s*int\s+Property\s+{name}\s*=\s*{value}\s+Auto\s+Const\b", source) is not None,
              f"{script}.psc must declare const {name} = {value}")


check_script("effect 0x000800", contract.get("effectScript", {}), "RadOutbreakEffect")
check_script("quest 0x000803", contract.get("questScript", {}), "RadOutbreakQuest")
doctor = contract.get("doctor", {})
check_script("doctor station", {"record": doctor.get("station"), "objectProperties": doctor.get("scriptProperties", {})}, doctor.get("script", ""))
check(f"CustomEvent {contract.get('questScript', {}).get('customEvent')}" in text(SOURCES / "RadOutbreakQuest.psc"),
      "RadOutbreakQuest.psc must declare the contract custom event")

build = json.loads(text(ROOT / "deploy/Scripts/papyrus-build.json") or "{}")
listed = {entry.get("script") for entry in build.get("scripts", [])}
for script in ("RadOutbreakEffect", "RadOutbreakQuest", "RadOutbreakDoctorStation"):
    check(script in listed, f"deploy/Scripts/papyrus-build.json must list {script}")

ini = text(ROOT / "deploy/F4SE/Plugins/FalloutAnomalyRadiation.ini")
policy = text(ROOT / "src/RadNeedsTuningPolicy.h")
tuning_test = text(ROOT / "tests/verify_needs_tuning.py")
specs = re.findall(r'\{\s*"(\w+)",\s*(-?[\d.]+)f,\s*(-?[\d.]+)f,\s*(-?[\d.]+)f,\s*(true|false)\s*\}', policy)
spec_names = [spec[0] for spec in specs]
for key in contract.get("tuning", []):
    name = key["name"]
    check(re.search(rf"(?m)^{name}=", ini) is not None, f"the ini must list tuning key {name}")
    check(name in spec_names and spec_names.index(name) == key["key"], f"{name} must sit at tuning key {key['key']} in RadNeedsTuningPolicy.h")
    if name in spec_names:
        _, reference, low, high, toggle = specs[spec_names.index(name)]
        check(same(float(reference), key["reference"]) and same(float(low), key["min"]) and same(float(high), key["max"])
              and (toggle == "true") == (key["kind"] == "toggle"),
              f"{name} reference, range and toggle in RadNeedsTuningPolicy.h must match the contract")
    check(f'"{name}"' in tuning_test, f"tests/verify_needs_tuning.py must list tuning key {name}")
check(contract.get("stages", {}).get("deathRequiresTuningKey") == 18 and contract.get("effectScript", {}).get("constProperties", {}).get("kTuneDeaths") == 18,
      "the death toggle must be tuning key 18 in the contract")

runtime = text(ROOT / "src/NeedsRuntime.cpp")
needs_policy = text(ROOT / "src/NeedsPolicy.h")
for letter in ("C", "E"):
    for sex in ("Male", "Female"):
        path = rf"Sound\\FX\\QST\\PlayerCryopod\\QST_Player_Cryopod_Breathing_{letter}_{sex}_01.xwm"
        check(path in runtime, f"NeedsRuntime.cpp must carry the vanilla cryopod cough {path}")
check("void NeedsRuntime::UpdateSickCough(const RadNeedsPolicy::NeedsSnapshot& snapshot)" in runtime and "UpdateSickCough(snapshot);" in runtime,
      "NeedsRuntime::UpdateSickCough must exist and run from the needs update")
check("RadNeedsPolicy::CoughDelayTicks(stage, roll(rng))" in runtime and "inline int CoughDelayTicks(int stage, float roll01) noexcept" in needs_policy,
      "the sick cough delay must come from RadNeedsPolicy::CoughDelayTicks")

held = text(ROOT / "src/OutbreakQuestHeld.cpp")
quest_psc = text(SOURCES / "RadOutbreakQuest.psc")
player_stage = quest_psc.split("int Function PlayerStage()", 1)[-1].split("EndFunction", 1)[0]
check("id != 1 && id != 2 && id != 3 && id != 6" in player_stage and "return -1" in player_stage,
      "only contagious player illnesses (fever, wasting, dysentery, pneumonia) may spread to NPCs")
check("bool Function IsQuestHeld(Form akActor) global native" in text(SOURCES / "RadNatives.psc")
      and 'BindNativeMethod("RadNatives", "IsQuestHeld", IsQuestHeld' in held and "OutbreakQuestHeld::Install();" in text(ROOT / "src/main.cpp"),
      "RadNatives.IsQuestHeld must be declared, bound and installed")
check("bool IsQuestHeld(std::monostate, RE::TESForm* a_form)" in held and "a_form->As<RE::Actor>()" in held
      and "#include <RE/T/TESFormUtil.h>" in held,
      "IsQuestHeld must take TESForm* (binds as Form; Actor* also binds as Form and mismatched the old Actor declaration) and cast to Actor")
check("GetByType<RE::ExtraAliasInstanceArray>()" in held and "kFollowersQuest = 0x0289E4" in held and "kWorkshopParentQuest = 0x02058E" in held
      and "quest == outbreak" in held,
      "IsQuestHeld must read the alias array and ignore Followers, WorkshopParent and RadOutbreak")
check("(IsCompanion(pool[j]) || !RadNatives.IsQuestHeld(pool[j]))" in quest_psc,
      "companions must bypass the quest-held filter: each one sits in its own running companion quest")

effect_psc = text(SOURCES / "RadOutbreakEffect.psc")
check("if (stage < 1 || stage > RadNeedsPolicy::kDiseaseMaxStage) { _coughTick = 0; return; }" in runtime,
      "UpdateSickCough must reset and return for stage 0 and for stages above kDiseaseMaxStage, as CoughDelayTicks does")
check(re.search(r"bool Property LogProbe = false Auto Const", effect_psc) is not None
      and re.search(r"bool Property LogProbe = false Auto Const", quest_psc) is not None,
      "LogProbe must ship false in RadOutbreakEffect.psc and RadOutbreakQuest.psc")
check(re.search(r"if !Patient\.IsInFaction\(SickFaction\)\s+Patient\.AddToFaction\(SickFaction\)\s+endif\s+Patient\.SetFactionRank\(SickFaction, Stage\)", effect_psc) is not None,
      "ApplyStage must add the patient to SickFaction before setting the rank")
check(len(re.findall(r"CancelTimerGameTime\(2\)\s+StartTimerGameTime\(1\.0, 2\)", effect_psc)) == 2,
      "both restarts of the effect's game-time timer 2 (the timer handler and command 6) must cancel it first")
check(re.search(r"EnsureRegistry\(\)\s+CancelTimerGameTime\(1\)\s+StartTimerGameTime\(1\.0, 1\)", quest_psc) is not None,
      "RunDebug must cancel the quest's game-time timer 1 before restarting it")
check(re.search(r"Patient\.RemoveSpell\(InfectionSpell\)\s+if OutbreakQuest != None\s+ClearInfection\(\)\s+endif", effect_psc) is not None
      and len(re.findall(r"^\s+ClearInfection\(\)", effect_psc, re.MULTILINE)) == 2 and "Function ClearInfection()" in effect_psc,
      "the OnEffectStart rejection path and OnEffectFinish must share ClearInfection")
check(re.search(r"Function IsResilient\(Actor akActor\)\s+return IsCompanion\(akActor\) \|\|[^\r\n]*RadNatives\.IsQuestHeld\(akActor\)", quest_psc) is not None,
      "IsResilient must treat a quest-held NPC as resilient so it never dies")

if failures:
    print("outbreak record contract failures:")
    for failure in failures:
        print(f"  FAIL {failure}")
    sys.exit(1)

print("Outbreak records verified: records, names, VMAD bindings, sounds, tuning keys and the sick cough")
