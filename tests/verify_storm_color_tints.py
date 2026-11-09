import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VIEW = ROOT / "deploy" / "PrismaUI_F4" / "views" / "Interface" / "FalloutAnomaly"
SRC = ROOT / "src"


def fail(message):
    raise SystemExit(message)


html = (VIEW / "radhud.html").read_text(encoding="utf-8")
css = (VIEW / "radhud.css").read_text(encoding="utf-8")
js = (VIEW / "radhud.js").read_text(encoding="utf-8")
actors = (SRC / "RadEngine_Storm_Actors.cpp").read_text(encoding="utf-8")
internal = (SRC / "RadEngine_Internal.h").read_text(encoding="utf-8", errors="replace")

if not re.search(r'<div\s+id="stormTint"\s*>\s*</div>', html):
    fail('radhud.html must contain <div id="stormTint"></div>')

tint_block = re.search(r"#stormTint\s*\{([^}]*)\}", css)
if not tint_block:
    fail("radhud.css must style #stormTint")
tint_css = tint_block.group(1)
if not re.search(r"position:\s*fixed", tint_css):
    fail("#stormTint must be position: fixed")
if not re.search(r"pointer-events:\s*none", tint_css):
    fail("#stormTint must be pointer-events: none (click-through)")
if "transition" not in tint_css:
    fail("#stormTint must transition so storm phases fade")
tint_z = re.search(r"z-index:\s*(\d+)", tint_css)
banner_block = re.search(r"#emissionBanner\s*\{([^}]*)\}", css)
banner_z = re.search(r"z-index:\s*(\d+)", banner_block.group(1)) if banner_block else None
if not tint_z or not banner_z or int(tint_z.group(1)) >= int(banner_z.group(1)):
    fail("#stormTint must sit below #emissionBanner in z-order")

rows = re.findall(r"(\d):\s*\{\s*rgb:\[(\d+),(\d+),(\d+)\],\s*cap:([\d.]+)\s*\}", js)
table = {int(i): ((int(r), int(g), int(b)), float(cap)) for i, r, g, b, cap in rows}
if set(table) != {1, 2, 3, 4, 5}:
    fail(f"radhud.js STORM_TINTS must define exactly themes 1-5, found {sorted(table)}")
if table[4][0] != (255, 255, 255):
    fail("Ash Fallout (theme 4) must tint white (255,255,255)")
if table[5][0] != (140, 100, 55):
    fail("Dust Storm (theme 5) must tint brown (140,100,55)")
for theme, (_, cap) in table.items():
    if not 0.10 <= cap <= 0.22:
        fail(f"theme {theme} alpha cap {cap} is outside 0.10-0.22")
if not (table[4][1] < min(table[1][1], table[5][1]) and table[2][1] < min(table[1][1], table[5][1])):
    fail("white ash and yellow sulfur caps must stay below red firestorm and brown dust")

if not re.search(r"function\s+stormTintFor\s*\(\s*theme\s*,\s*phase\s*,\s*intensity\s*,\s*sheltered\s*\)", js):
    fail("radhud.js must define stormTintFor(theme, phase, intensity, sheltered)")
if not re.search(r"if\s*\(\s*!t\s*\|\|\s*sheltered\s*\|\|\s*phase\s*<\s*1\s*\|\|\s*phase\s*>\s*3\s*\)\s*return\s+off", js):
    fail("stormTintFor must return zero alpha when untinted, sheltered, Idle (0) or Approach (4)")
if "stormTintFor(theme, phase, d.intensity, shelter)" not in js:
    fail("stormPhase must feed the tint from the pushed payload")

ash_index = re.search(r"kAshStormIndex\s*=\s*(\d+)", internal)
dust_index = re.search(r"kDustStormIndex\s*=\s*(\d+)", internal)
if not ash_index or ash_index.group(1) != "0" or not dust_index or dust_index.group(1) != "4":
    fail("kAshStormIndex must be 0 and kDustStormIndex must be 4")
if not re.search(r'\{\s*0x840,\s*"Ash Fallout",\s*4,', internal):
    fail("Ash Fallout must keep theme id 4")
if not re.search(r'\{\s*0x844,\s*"Dust Storm",\s*5,', internal):
    fail("Dust Storm must keep theme id 5")

helper = re.search(
    r"bool\s+StormTypeUsesEngineIMAD\(int typeIndex\)\s*\{\s*return\s+typeIndex\s*!=\s*kAshStormIndex\s*&&\s*typeIndex\s*!=\s*kDustStormIndex;\s*\}",
    actors,
)
if not helper:
    fail("Storm_Actors.cpp must exclude Ash and Dust from the engine IMAD via StormTypeUsesEngineIMAD")
if "typeIndex >= kStormTypeCount || !StormTypeUsesEngineIMAD(typeIndex)" not in actors:
    fail("SetStormIMADTarget must fade out instead of starting the IMAD for excluded storm types")
if "typeIndex == kDustStormIndex" in actors:
    fail("the old dust-only IMAD test must be replaced, not kept beside the helper")

print("Storm colour tints verified: stormTint overlay is click-through and below the banner, five theme colours, caps 0.10-0.22, white ash and brown dust, Idle/Approach/sheltered hidden, Ash and Dust excluded from the engine IMAD")
