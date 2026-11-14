"""Removes named records from a plain ESP and leaves every other byte alone.

The tool walks the GRUP tree, drops the records named by --remove, optionally drops one VMAD script
property with --drop-vmad-property, and rebuilds only the enclosing GRUP headers. A GRUP left with no
children is removed too. TES4 HEDR NumRecords is recomputed; the next object id and version are never
touched.

Kept records are copied byte for byte. The only exception is a record named by --drop-vmad-property,
whose VMAD subrecord is spliced; the report lists it explicitly.

Before writing, the run stops if any kept record still references a removed form. The check scans the
raw bytes of every kept record for the little-endian form id, so a hit names the record that holds it.

HEDR NumRecords convention, checked against the shipped file before this tool was written: the count
is every record including TES4, plus every GRUP. RadiationOverhaul.esp reads 238 under that rule.

Usage:
  python tools/remove_plugin_records.py deploy/RadiationOverhaul.esp --remove 03000805 ... \
      [--drop-vmad-property 03000871:RadStormFX.DomeZOffset] [--report docs/proof/x.json] [--dry-run]
"""

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
import plugin_reader as pr  # noqa: E402


class RemovalError(Exception):
    """Raised when the requested edit cannot be made safely."""


def hexid(value):
    return f"{value:08X}"


def count_records_and_groups(nodes):
    return sum(1 for _ in pr.walk(nodes))


def read_hedr_count(data, nodes):
    for sig, start, _size in nodes[0].subrecords(data):
        if sig == b"HEDR":
            return struct.unpack_from("<I", data, start + 4)[0]
    raise RemovalError("TES4 has no HEDR")


def drop_vmad_property(raw, script_name, prop_name):
    """Return VMAD bytes with one property removed; the rest is copied unchanged."""
    version, _obj_format, script_count = struct.unpack_from("<HHH", raw, 0)
    if version != 6:
        raise RemovalError(f"VMAD version {version} is not modelled")
    off = 6
    for _ in range(script_count):
        (name_len,) = struct.unpack_from("<H", raw, off)
        name = raw[off + 2:off + 2 + name_len].decode("ascii", "replace")
        off += 2 + name_len + 1
        count_at = off
        (prop_count,) = struct.unpack_from("<H", raw, off)
        off += 2
        for _ in range(prop_count):
            start = off
            (plen,) = struct.unpack_from("<H", raw, off)
            pname = raw[off + 2:off + 2 + plen].decode("ascii", "replace")
            off += 2 + plen
            ptype = raw[off]
            off += 2
            if ptype == 1:
                off += 8
            elif ptype == 2:
                (vlen,) = struct.unpack_from("<H", raw, off)
                off += 2 + vlen
            elif ptype in (3, 4):
                off += 4
            elif ptype == 5:
                off += 1
            else:
                raise RemovalError(f"VMAD property {pname} has unmodelled type {ptype}")
            if name == script_name and pname == prop_name:
                out = bytearray(raw[:start] + raw[off:])
                struct.pack_into("<H", out, count_at, prop_count - 1)
                return bytes(out)
    raise RemovalError(f"{script_name}.{prop_name} is not on that VMAD")


def rebuild_record(buf, node, new_vmad):
    header = bytearray(buf[node.offset:node.offset + 24])
    body = bytearray()
    off = node.data_start
    while off < node.data_end:
        sig = bytes(buf[off:off + 4])
        (size,) = struct.unpack_from("<H", buf, off + 4)
        if sig == b"VMAD":
            body += sig + struct.pack("<H", len(new_vmad)) + new_vmad
        else:
            body += bytes(buf[off:off + 6 + size])
        off += 6 + size
    struct.pack_into("<I", header, 4, len(body))
    return bytes(header) + bytes(body)


def emit(buf, node, removed, vmad_edits, changed):
    """New bytes for a node, or None when the node is removed or empty."""
    if node.kind == "record":
        fid = node.form_id(buf)
        if fid in removed:
            return None
        if fid in vmad_edits:
            script, prop = vmad_edits[fid]
            raw = node.field(buf, b"VMAD")
            if raw is None:
                raise RemovalError(f"{hexid(fid)} has no VMAD to edit")
            new_raw = drop_vmad_property(raw, script, prop)
            changed.append({"record": hexid(fid), "sig": node.sig.decode(), "edit": f"drop {script}.{prop}",
                            "vmadBytesBefore": len(raw), "vmadBytesAfter": len(new_raw)})
            return rebuild_record(buf, node, new_raw)
        return bytes(buf[node.offset:node.end])
    parts = [emit(buf, kid, removed, vmad_edits, changed) for kid in node.kids]
    parts = [p for p in parts if p is not None]
    if not parts:
        return None
    body = b"".join(parts)
    header = bytearray(buf[node.offset:node.offset + 24])
    struct.pack_into("<I", header, 4, 24 + len(body))
    return bytes(header) + body


def reference_hits(buf, nodes, targets, removed):
    """Kept records whose raw bytes contain the little-endian form id of a removed record."""
    hits = []
    for node in pr.walk(nodes):
        if node.kind != "record" or node.sig == b"TES4":
            continue
        fid = node.form_id(buf)
        if fid in removed:
            continue
        body = bytes(buf[node.data_start:node.data_end])
        for target in targets:
            if struct.pack("<I", target) in body:
                hits.append(f"{hexid(fid)} {node.sig.decode()} {pr.editor_id(buf, node) or ''} "
                            f"holds {hexid(target)}")
    return hits


def verify(original, new_bytes, removed, vmad_edits, expected_count):
    old_nodes = pr.parse(original)
    new_nodes = pr.parse(new_bytes)
    old_records = {n.form_id(original): n for n in pr.walk(old_nodes) if n.kind == "record" and n.sig != b"TES4"}
    new_records = {n.form_id(new_bytes): n for n in pr.walk(new_nodes) if n.kind == "record" and n.sig != b"TES4"}
    for fid in removed:
        if fid in new_records:
            raise RemovalError(f"{hexid(fid)} still present after removal")
    if set(new_records) != set(old_records) - set(removed):
        raise RemovalError("kept record set differs from original minus removed")
    report = []
    for fid, old in old_records.items():
        if fid in removed:
            continue
        new = new_records[fid]
        before = hashlib.sha256(original[old.offset:old.end]).hexdigest()
        after = hashlib.sha256(new_bytes[new.offset:new.end]).hexdigest()
        report.append({"record": hexid(fid), "sig": old.sig.decode(), "sha256Before": before,
                       "sha256After": after, "identical": before == after, "vmadEdited": fid in vmad_edits})
    unexpected = [r["record"] for r in report if not r["identical"] and not r["vmadEdited"]]
    if unexpected:
        raise RemovalError(f"unexpected byte change in kept records {unexpected}")
    real = count_records_and_groups(new_nodes)
    if real != expected_count:
        raise RemovalError(f"HEDR count {expected_count} does not match real count {real}")
    return report, real


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("esp")
    ap.add_argument("--remove", nargs="+", required=True, metavar="RECORD_ID")
    ap.add_argument("--drop-vmad-property", action="append", default=[], metavar="RECORD_ID:SCRIPT.PROP")
    ap.add_argument("--report", metavar="JSON")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)

    path = Path(args.esp)
    original = path.read_bytes()
    nodes = pr.parse(original)
    records = {n.form_id(original) for n in pr.walk(nodes) if n.kind == "record"}
    removed = {int(x, 16) for x in args.remove}
    missing = [hexid(f) for f in removed if f not in records]
    if missing:
        raise RemovalError(f"not in {path.name}: {missing}")

    vmad_edits = {}
    for item in args.drop_vmad_property:
        fid_text, _, ref = item.partition(":")
        script, _, prop = ref.partition(".")
        if not (fid_text and script and prop):
            raise RemovalError(f"bad --drop-vmad-property {item!r}")
        vmad_edits[int(fid_text, 16)] = (script, prop)
    if removed & set(vmad_edits):
        raise RemovalError("a record cannot be removed and VMAD-edited in one run")

    hits = reference_hits(original, nodes, removed, removed)
    if hits:
        raise RemovalError("kept records still reference removed forms: " + "; ".join(hits))

    changed = []
    parts = [emit(original, node, removed, vmad_edits, changed) for node in nodes]
    body = b"".join(p for p in parts if p is not None)
    new_nodes = pr.parse(body)
    expected = count_records_and_groups(new_nodes)
    buf = bytearray(body)
    for sig, start, _size in new_nodes[0].subrecords(buf):
        if sig == b"HEDR":
            struct.pack_into("<I", buf, start + 4, expected)
            break
    new_bytes = bytes(buf)

    report, real = verify(original, new_bytes, removed, vmad_edits, expected)
    groups_before = sum(1 for n in pr.walk(nodes) if n.kind == "group")
    groups_after = sum(1 for n in pr.walk(new_nodes) if n.kind == "group")
    summary = {
        "source": path.as_posix(),
        "removedRecords": sorted(hexid(f) for f in removed),
        "removedGroups": groups_before - groups_after,
        "vmadEdits": changed,
        "hedrNumRecordsBefore": read_hedr_count(original, nodes),
        "hedrNumRecordsAfter": real,
        "bytesBefore": len(original),
        "bytesAfter": len(new_bytes),
        "keptRecordCount": len(report),
        "keptRecordsIdentical": sum(1 for r in report if r["identical"]),
        "keptRecordsChanged": [r["record"] for r in report if not r["identical"]],
        "keptRecords": report,
    }
    print(f"{path.name}: removed {len(removed)} records and {summary['removedGroups']} empty GRUPs; "
          f"kept {len(report)} records ({summary['keptRecordsIdentical']} byte-identical); "
          f"HEDR NumRecords {summary['hedrNumRecordsBefore']} -> {real}; "
          f"{len(original)} -> {len(new_bytes)} bytes")
    if args.report:
        Path(args.report).write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if args.dry_run:
        print("dry run: nothing written")
        return 0
    path.write_bytes(new_bytes)
    print("written")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RemovalError, pr.PluginError) as exc:
        raise SystemExit(f"remove_plugin_records: {exc}")
