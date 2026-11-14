#!/usr/bin/env python3
"""Set record display names the FO4IDE writer cannot.

The native writer refuses Authoring\\FULL on every record type, so records it
creates (the cure ingestible, disease perks) have no Pip-Boy name. This rewrites
the plugin in place: for each EDID in the manifest it inserts or replaces that
record's FULL subrecord (dropping duplicates), sets an optional description (SPEL
DESC, otherwise DNAM; a named MGEF without one gets its DNAM blanked), then
rebuilds every record dataSize and group size so the file stays loadable.
All other bytes are copied verbatim.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

RECORD_HEADER = 24
GROUP_HEADER = 24


def read_subrecords(data: bytes) -> list[tuple[bytes, bytes]]:
    subs: list[tuple[bytes, bytes]] = []
    offset = 0
    while offset + 6 <= len(data):
        sig = data[offset : offset + 4]
        size = struct.unpack_from("<H", data, offset + 4)[0]
        offset += 6
        if sig == b"XXXX":
            size = struct.unpack_from("<I", data, offset)[0]
            offset += 4
            continue
        subs.append((sig, data[offset : offset + size]))
        offset += size
    if offset != len(data):
        raise ValueError(f"subrecord walk ended at {offset} of {len(data)} bytes")
    return subs


def encode_subrecords(subs: list[tuple[bytes, bytes]]) -> bytes:
    out = bytearray()
    for sig, payload in subs:
        if len(payload) > 0xFFFF:
            out += b"XXXX" + struct.pack("<HI", 4, len(payload))
        out += sig + struct.pack("<H", len(payload)) + payload
    return bytes(out)


class Node:
    def __init__(self, kind: str, header: bytes, payload: bytes, children: list["Node"] | None = None):
        self.kind = kind
        self.header = header
        self.payload = payload
        self.children = children or []


def parse(data: bytes, start: int, end: int) -> list[Node]:
    nodes: list[Node] = []
    offset = start
    while offset < end:
        sig = data[offset : offset + 4]
        if sig == b"GRUP":
            size = struct.unpack_from("<I", data, offset + 4)[0]
            header = data[offset : offset + GROUP_HEADER]
            nodes.append(Node("group", header, b"", parse(data, offset + GROUP_HEADER, offset + size)))
            offset += size
        else:
            size = struct.unpack_from("<I", data, offset + 4)[0]
            header = data[offset : offset + RECORD_HEADER]
            nodes.append(Node("record", header, data[offset + RECORD_HEADER : offset + RECORD_HEADER + size]))
            offset += RECORD_HEADER + size
    if offset != end:
        raise ValueError(f"record walk ended at {offset} of {end} bytes")
    return nodes


def serialize(nodes: list[Node]) -> bytes:
    out = bytearray()
    for node in nodes:
        if node.kind == "group":
            body = serialize(node.children)
            header = bytearray(node.header)
            struct.pack_into("<I", header, 4, GROUP_HEADER + len(body))
            out += header + body
        else:
            payload = node.payload
            header = bytearray(node.header)
            struct.pack_into("<I", header, 4, len(payload))
            out += header + payload
    return bytes(out)


def rename(plugin: Path, names: dict[str, str], descriptions: dict[str, str] | None = None) -> list[str]:
    data = plugin.read_bytes()
    if data[:4] == b"TES4":
        first = RECORD_HEADER + struct.unpack_from("<I", data, 4)[0]
    else:  # pragma: no cover
        raise ValueError("plugin does not start with a TES4 header")
    nodes = parse(data, first, len(data))

    descriptions = descriptions or {}
    renamed: list[str] = []

    def visit(items: list[Node]) -> None:
        for node in items:
            if node.kind == "group":
                visit(node.children)
                continue
            subs = read_subrecords(node.payload)
            edid = next((payload for sig, payload in subs if sig == b"EDID"), None)
            if edid is None:
                continue
            editor_id = edid.rstrip(b"\x00").decode("ascii", "replace")
            wanted = names.get(editor_id)
            described = descriptions.get(editor_id)
            if wanted is None and described is None:
                continue
            result: list[tuple[bytes, bytes]] = list(subs)
            if wanted is not None:
                encoded = wanted.encode("utf-8") + b"\x00"
                at = next((i for i, (sig, _) in enumerate(result) if sig == b"FULL"), None)
                if at is None:
                    at = next(i for i, (sig, _) in enumerate(result) if sig == b"EDID") + 1
                result = result[:at] + [(b"FULL", encoded)] + [(sig, payload) for sig, payload in result[at:] if sig != b"FULL"]
                if node.header[:4] == b"MGEF" and described is None:
                    result = [(sig, b"\x00" if sig == b"DNAM" else payload) for sig, payload in result]
            if described is not None:
                dsig = b"DESC" if node.header[:4] == b"SPEL" else b"DNAM"
                encoded_desc = described.encode("utf-8") + b"\x00"
                if any(sig == dsig for sig, _ in result):
                    result = [(dsig, encoded_desc) if sig == dsig else (sig, payload) for sig, payload in result]
                else:
                    at = next((i for i, (sig, _) in enumerate(result) if sig == b"CTDA"), len(result))
                    result.insert(at, (dsig, encoded_desc))
            node.payload = encode_subrecords(result)
            renamed.append(editor_id)

    visit(nodes)
    if renamed:
        plugin.write_bytes(data[:first] + serialize(nodes))
    return renamed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    names = manifest.get("names", {})
    descriptions = manifest.get("descriptions", {})
    renamed = rename(args.plugin, names, descriptions)
    missing = sorted((set(names) | set(descriptions)) - set(renamed))
    for editor_id in renamed:
        print(f"named {editor_id}: {names.get(editor_id)!r} desc={descriptions.get(editor_id)!r}")
    for editor_id in missing:
        print(f"missing record {editor_id} (not in {args.plugin.name})", file=sys.stderr)
    return 0 if not missing else 1


if __name__ == "__main__":
    raise SystemExit(main())
