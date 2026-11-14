"""Contract for thresholds.json: the file holds only what RadiationConfig::LoadFromFile reads."""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHIPPED = ROOT / "deploy/F4SE/Plugins/FalloutAnomaly/thresholds.json"
COPY = ROOT / "data/thresholds.json"
LOADER = ROOT / "src/RadiationConfig.cpp"
HEADER = ROOT / "src/RadiationEngine.h"

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


shipped = json.loads(SHIPPED.read_text(encoding="utf-8"))
copy = json.loads(COPY.read_text(encoding="utf-8"))
check(shipped == copy, "data/thresholds.json must match the shipped deploy copy")

stages = shipped.get("stages", {})
stage_count = int(re.search(r"kStageCount = (\d+);", HEADER.read_text(encoding="utf-8")).group(1))
check(sorted(stages, key=int) == [str(n * 100) for n in range(stage_count + 1)],
      f"stage keys must be 0..{stage_count * 100} in steps of 100, one per kStagePenalties entry plus healthy")
for key, stage in stages.items():
    check(set(stage) == {"name"}, f"stage {key} must hold only 'name', the one field the loader reads: {sorted(stage)}")
    check(isinstance(stage.get("name"), str) and stage["name"].strip(), f"stage {key} needs a non-empty name")

loader = LOADER.read_text(encoding="utf-8")
read_fields = set(re.findall(r'data\.value\("(\w+)"', loader))
check(read_fields == {"name"}, f"RadiationConfig::LoadFromFile reads {sorted(read_fields)}; update thresholds.json and this test together")

if failures:
    for failure in failures:
        print("  FAIL", failure)
    raise SystemExit(1)

print(f"thresholds.json verified: {len(stages)} stages, name only, both copies identical")
