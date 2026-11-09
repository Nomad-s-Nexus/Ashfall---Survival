"""Pins the removal of the storm dome plugin records and the Papyrus code that used them.

The records were removed by tools/remove_plugin_records.py on 2026-11-07. This check reads the shipped
ESP and the compiled RadStormFX.pex and fails if a dome record, a dome reference, the dropped VMAD
property, or the HEDR count comes back. Other records are not asserted here; the other verify_* tests
cover them.
"""

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
import plugin_reader as pr  # noqa: E402

DOME_RECORDS = {
    0x03000805: "ACTI FARO_StormDome01",
    0x03000806: "ACTI FARO_StormDome02",
    0x03000807: "ACTI FARO_StormDome03",
    0x03000808: "ACTI FARO_StormDome04",
    0x03000872: "STAT RadOverhaul_StormDome",
    0x03000887: "GDRY FARO_StormDomeGodRays",
    0x03000888: "LENS FARO_StormDomeLensFlare",
    0x03000889: "LIGH FARO_StormDomeLight",
    0x0300088A: "LIGH FARO_StormDomeFlare",
}
STORM_FX_QUEST = 0x03000871
DROPPED_PROPERTY = "DomeZOffset"


def check(condition, message):
    if not condition:
        raise SystemExit(f"dome records: {message}")


def main():
    esp = ROOT / "deploy/RadiationOverhaul.esp"
    buf = esp.read_bytes()
    nodes = pr.parse(buf)
    everything = list(pr.walk(nodes))
    records = [n for n in everything if n.kind == "record"]
    groups = [n for n in everything if n.kind == "group"]

    present = {n.form_id(buf): n for n in records if n.form_id(buf) in DOME_RECORDS}
    check(not present, f"dome records still in the ESP: {[DOME_RECORDS[f] for f in present]}")

    for node in records:
        if node.sig == b"TES4":
            continue
        body = bytes(buf[node.data_start:node.data_end])
        for form_id, label in DOME_RECORDS.items():
            check(struct.pack("<I", form_id) not in body,
                  f"{node.sig.decode()} {pr.editor_id(buf, node)} still references {label}")

    hedr = nodes[0].field(buf, b"HEDR")
    check(hedr is not None, "TES4 has no HEDR")
    (_version, num_records, _next_id) = struct.unpack_from("<fII", hedr, 0)
    check(num_records == len(records) + len(groups),
          f"HEDR NumRecords {num_records} is not the real count {len(records) + len(groups)}")

    quest = next((n for n in records if n.form_id(buf) == STORM_FX_QUEST), None)
    check(quest is not None, "RadOverhaul_StormFXQuest is missing")
    scripts = pr.vmad_properties(buf, quest)
    check("RadStormFX" in scripts, "RadStormFX is no longer attached to the storm FX quest")
    check(DROPPED_PROPERTY not in scripts["RadStormFX"],
          f"the ESP still stores RadStormFX.{DROPPED_PROPERTY}")

    source = (ROOT / "deploy/Scripts/Source/User/RadStormFX.psc").read_text(encoding="utf-8")
    for token in ("EnableCloudDome", "DomeZOffset", "DomeFollowInterval", "_SpawnDome", "_RemoveDome",
                  "_kDomeTimer", "GetFormFromFile(205", "FARO_StormDome"):
        check(token not in source, f"RadStormFX.psc still contains {token}")

    pex = (ROOT / "deploy/Scripts/RadStormFX.pex").read_bytes()
    for token in (b"EnableCloudDome", b"DomeZOffset", b"_SpawnDome", b"FARO_StormDome"):
        check(token not in pex, f"RadStormFX.pex still contains {token.decode()}")

    print(f"Dome records removed: {len(DOME_RECORDS)} absent, no references left, "
          f"HEDR NumRecords {num_records} matches, RadStormFX has no dome code or dome property")


if __name__ == "__main__":
    main()
