#!/usr/bin/env python3
"""Contract for the outbreak detector and companion sickness: runtime scan, HUD widgets, debug tab and record IDs."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
VIEW = ROOT / "deploy/PrismaUI_F4/views/Interface/FalloutAnomaly"
failures: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.is_file() else ""


def local_id(value: object) -> int | None:
    if isinstance(value, int) and not isinstance(value, bool):
        return value & 0x00FFFFFF
    if isinstance(value, str) and re.fullmatch(r"(0x)?[0-9A-Fa-f]{1,8}", value.strip()):
        return int(value.strip(), 16) & 0x00FFFFFF
    return None


runtime = text(SRC / "OutbreakRuntime.cpp")
runtime_h = text(SRC / "OutbreakRuntime.h")
hud = text(SRC / "RadHudOutbreak.cpp")
hud_h = text(SRC / "RadHud.h")
life = text(SRC / "RadHudLifecycle.cpp")
hud_main = text(SRC / "RadHud.cpp")
hooks = text(SRC / "Hooks.cpp")
html = text(VIEW / "hud_outbreak.html")
contract_path = ROOT / "tools/rad_outbreak.json"

check('constexpr const char* kPlugin = "RadOutbreak.esp";' in runtime, "the detector must resolve forms from RadOutbreak.esp")
check("LookupForm(kSickFactionForm, kPlugin)" in runtime and "LookupForm(kScannerForm, kPlugin)" in runtime,
      "both forms must be resolved by local ID through TESDataHandler")
check("ENUM_FORM_ID::kFACT" in runtime and "ENUM_FORM_ID::kMISC" in runtime, "resolved forms must be type-checked")
check("if (!player || !player->GetParentCell() || !processLists)" in runtime and 'PushOutbreak("[]")' in runtime and "if (!markersOn)" in runtime,
      "an absent plugin or scanner must clear the markers and do nothing else")
check("constexpr float kScannerRange = 4096.0f;" in runtime, "scanner range is 4096 units")
check("constexpr std::uint32_t kVanillaScannerForm = 0x154AD3;" in runtime and "LookupForm(kVanillaScannerForm, kCompanionPlugin)" in runtime,
      "the vanilla Biometric Scanner (154AD3:Fallout4.esm) must also turn the markers on")
check("GetInventoryObjectCount(" in runtime, "the scanner must be a carried item")
check("highActorHandles" in runtime and "IsDead(false)" in runtime and "IsPlayerRef()" in runtime,
      "markers cover loaded living actors other than the player")
check("GetByType<RE::ExtraFactionChanges>()" in runtime and "changes->factionChanges" in runtime,
      "the sick-faction rank is read from the reference's faction changes")
check("constexpr int kLastSickRank = 3;" in runtime and "!markersOn || stage > kLastSickRank" in runtime,
      "only ranks 0..3 are marked; rank 4 is immune")
check("RE::Main::WorldRootCamera()" in runtime and "camera->worldToCam" in runtime and "w <= 0.00001f" in runtime,
      "markers project through the world camera and drop points behind it")
check("x < -kEdgeMargin" in runtime and "y > 1.0f + kEdgeMargin" in runtime, "off-screen points must be dropped")
check("kMaxMarkers" in runtime, "the marker count must be capped")
check('step("Outbreak", [] { OutbreakRuntime::GetSingleton().Update(); });' in hooks,
      "the detector must run on the 4 Hz engine tick")
check("static constexpr float kUpdateInterval = 0.25f;" in hooks, "the engine tick is 4 Hz")
check("void Reset();" in runtime_h, "the detector must reset with the other runtimes")
for source in (text(SRC / "main.cpp"), text(SRC / "RadEngine_Serialization.cpp")):
    check("OutbreakRuntime::GetSingleton().Reset();" in source, "every runtime reset site must reset the detector")

check("Interface/FalloutAnomaly/hud_outbreak.html" in hud, "the HUD must load the outbreak view")
check('api->InteropCall(_outbreakView, "updateOutbreak", payload.c_str());' in hud, "the HUD must feed updateOutbreak")
check("if (payload == _outbreakPayload) return;" in hud, "an unchanged payload must not be pushed again")
check("if (empty || !PlayerInWorld() || !LoadSettled()) return;" in hud, "the view is created only when there is something to draw")
check("_coverHidden[kDomSlotOutbreak] = 0;" in hud, "a HUD-initiated hide must clear the menu-cover slot")
check("RegisterWidget" not in hud, "the outbreak overlay is full-screen, not a movable layout widget")
check("_outbreakView, _companionsView };" in hud_main, "the menu cover must include the outbreak view")
check("FinalizeOutbreakDomReady(pendingOutbreak)" in life, "the DOM-ready pump must finalize the outbreak view")
check("HideOutbreak();" in hud_main, "a session reset must hide the markers")

check("window.updateOutbreak" in html and "window.setTheme" in html, "the overlay must expose updateOutbreak and setTheme")
check("pointer-events:none" in html, "the overlay must never take input")
check("COLORS" not in html and 'data-s="1"]{--c:var(--amber)}' in html and 'data-s="2"],.mark[data-s="3"]{--c:var(--rad)}' in html
      and "--c:var(--muted)" in html,
      "scanner stage colours are the PhosCore tokens: muted, amber, rad, rad")
check("width:26px;height:26px" in html and "var MAX=24,SEGS=4,GAP=16" in html and "'<path class=\"seg\" d=\"'+arc(k*90+GAP/2,(k+1)*90-GAP/2)" in html
      and "A28 28 0 0 1" in html and 'class="disc" cx="32" cy="32" r="24.5"' in html and 'class="dot" cx="32" cy="32" r="5"' in html,
      "the scanner marker is a 26px PhosCore ring of four arc segments with a dark disc and a centre dot")
check("stroke:rgba(220,245,223,.13)" in html and ".mark .seg.on{stroke:var(--c)}" in html and "k<=stage?'seg on':'seg'" in html,
      "segments up to the actor stage are lit in the stage colour; the rest use the PhosCore track alpha")
check('.mark[data-s="3"] .seg.on,.mark[data-s="3"] .dot{animation:blink 1s steps(2,start) infinite}' in html and "@keyframes blink{to{opacity:.35}}" in html
      and "@media (prefers-reduced-motion:reduce)" in html and "animation:none" in html,
      "stage 3 blinks with the PhosCore steps(2) blink and honours prefers-reduced-motion")
check("--pip:#93e8a4;--cyan:#86e6ea;--amber:#f2c46a;--rad:#ff7052;--ink:#dcf5df;--muted:#8fa996" in html
      and not any(old in html + text(VIEW / "hud_companions.html") for old in ("#9aa0a6", "#f2d14b", "#f28c28", "#e5484d", "#5fd6b4")),
      "only the PhosCore tokens are used; the off-theme colours are gone from both views")
check("e._segs=e.querySelectorAll('.seg')" in html and "pool[i]=e" in html, "marker nodes stay pooled")
check("var MAX=24" in html and "kMaxMarkers = 24" in runtime, "the overlay and the runtime must agree on the marker cap")

check(contract_path.is_file(), "tools/rad_outbreak.json must record the faction and scanner form IDs")
if contract_path.is_file():
    contract = json.loads(text(contract_path))
    check(contract.get("scanner", {}).get("vanillaItem") == "0x154AD3:Fallout4.esm",
          "tools/rad_outbreak.json must record the vanilla scanner item")
    expected_ids = {
        "kSickFactionForm": local_id(contract.get("faction", {}).get("localId")),
        "kScannerForm": local_id(contract.get("scanner", {}).get("localId")),
    }
    for name, expected in expected_ids.items():
        match = re.search(rf"constexpr std::uint32_t {name} = (0x[0-9A-Fa-f]+);", runtime)
        check(expected is not None, f"tools/rad_outbreak.json must carry {name} at its faction/scanner localId path")
        check(match is not None and expected is not None and int(match.group(1), 16) == expected,
              f"{name} must equal the localId in tools/rad_outbreak.json")
    check(expected_ids["kSickFactionForm"] == 0x000802 and expected_ids["kScannerForm"] == 0x000813,
          "the contract must carry the sick faction 0x000802 and the scanner 0x000813")
    companion_ref = contract.get("questScript", {}).get("objectProperties", {}).get("CompanionFaction")
    match = re.search(r"constexpr std::uint32_t kCompanionFactionForm = (0x[0-9A-Fa-f]+);", runtime)
    check(companion_ref == "0x023C01:Fallout4.esm", "the contract must name CurrentCompanionFaction as 0x023C01:Fallout4.esm")
    check(match is not None and int(match.group(1), 16) == 0x023C01 and 'kCompanionPlugin = "Fallout4.esm"' in runtime,
          "kCompanionFactionForm must be 0x023C01 resolved from Fallout4.esm")

widget_cpp = text(SRC / "RadHudCompanionStatus.cpp")
widget_html = text(VIEW / "hud_companions.html")
layout_cpp = text(SRC / "RadHudLayout.cpp")
companion_cpp = text(SRC / "RadHudCompanion.cpp")
editor_js = text(VIEW / "rad_hud_local_editor.js")
debug_html = text(VIEW / "rad_debug.html")
outbreak_js = text(VIEW / "rad_debug_outbreak.js")
debug_js = text(VIEW / "rad_debug.js") + outbreak_js

check("LookupForm(kCompanionFactionForm, kCompanionPlugin)" in runtime, "the companion faction must be resolved by local ID from Fallout4.esm")
check("if (_faction && _companion && _scanner && _vanillaScanner)" in runtime and "if (!_companion)" in runtime and "if (!_scanner)" in runtime
      and "if (!_vanillaScanner)" in runtime,
      "the sick faction, companion faction, scanner and vanilla scanner must resolve independently")
check("const bool  markersOn = sick && (carries(_scanner) || carries(_vanillaScanner));" in runtime and "if (sick || companionFaction)" in runtime,
      "companion collection must run without the scanner; only the markers need it")
check("FactionRank(actor, companionFaction) >= 0" in runtime and "a_actor->GetNPC()" in runtime and "npc->factions" in runtime,
      "a companion is any live loaded actor with rank >= 0 in the companion faction, from reference changes or the base list")
check("constexpr int kImmuneRank = 4;" in runtime and "++nearby[stage];" in runtime and "int                    nearby[kImmuneRank + 1]{};" in runtime,
      "nearby sick actors are counted per stage 0-4")
check("if (distanceSq > kScannerRange * kScannerRange)" in runtime and runtime.index("++nearby[stage];") > runtime.index("distanceSq > kScannerRange"),
      "the nearby count only covers actors within 4096 units")
check("hud.PushCompanions(widget.dump());" in runtime and "if (sick)" in runtime,
      "the companion widget gets the list only when the sick faction resolved")
check("if (hud.DebugMenuOpen())" in runtime and "hud.PushOutbreakDebug(" in runtime, "the debug status is built only while the debug menu is open")
check('"companions"' in runtime and '"nearby"' in runtime, "the debug payload must carry companions and nearby counts")
check("kWidgetCompanions = 3" in runtime and "MAX=3" in widget_html.replace(" ", ""), "the runtime and the widget agree on three companion cores beside the player core")

check("kDomSlotOutbreak = 6" in hud_h and "kDomSlotCompanions = 7" in hud_h and "kDomSlotCount = 8" in hud_h
      and "kCoverViewCount = kDomSlotCount" in hud_h, "the companion view needs its own DOM slot and cover entry")
check("{ _view, _toxicView, _filterView, _climateView, _needsView, 0, _outbreakView, _companionsView };" in hud_main,
      "the menu cover array must be indexed by DOM slot")
check("HideCompanions();" in hud_main and "_outbreakDebugPayload.clear();" in hud_main, "a session reset hides the companions and a new debug view resets its payload")
check("FinalizeCompanionsDomReady(pendingCompanions)" in life, "the DOM-ready pump must finalize the companion view")
check("Interface/FalloutAnomaly/hud_companions.html" in widget_cpp and 'api->InteropCall(_companionsView, "updateCompanions", payload.c_str());' in widget_cpp,
      "the HUD must load and feed the companion widget")
check("if (_companionsPayload == payload) return;" in widget_cpp and "!PlayerInWorld() || !LoadSettled()" in widget_cpp,
      "an unchanged payload is not re-pushed and the view waits for the settle")
check('const bool empty = list == "[]" && _youDisease == 0 && !always;' in widget_cpp and "if (empty || !PlayerInWorld() || !LoadSettled()) return;" in widget_cpp,
      "the companion view is created only when there is a companion, a sick player or the always-show key")
check("kAlwaysShowNeeds" in widget_cpp, "the companion widget follows the always-show-needs tuning key")
check('hud->RegisterWidget(_companionsView, CompanionPlaced({ "rad_companions", "Companions: sickness",' in widget_cpp
      and "hud->ApplyLayout(_companionsView);" in widget_cpp and "SyncLocalHudLayout();" in widget_cpp,
      "rad_companions registers with PrismaHUD when present and syncs the local layout editor otherwise")
check('InteropCall(_debugView, "updateOutbreakStatus"' in widget_cpp and "!_debugOpen" in widget_cpp, "the debug tab is fed only while it is open")

check('{ "rad_companions", 0.0f, 200.0f }' in companion_cpp, "the companion-mode table must place rad_companions")
check("WidgetLayout companions{ 22.0f / 1920.0f, 530.0f / 1080.0f," in layout_cpp, "the own-vitals default slot for rad_companions")
for needle in ('{ "rad_companions", WidgetJson(g_layout.companions) }', '{ "rad_companions", WidgetJson(base.companions) }',
               'saved["rad_companions"], g_layout.companions', 'if (id == "rad_companions") return &g_layout.companions;',
               'CompanionWidgetDefault("rad_companions", state.companions.x, state.companions.y);'):
    check(needle in layout_cpp, f"RadHudLayout.cpp must carry rad_companions: {needle}")
check("rad_companions: { name:" in editor_js and 'cardHtml("rad_companions")' in editor_js and editor_js.count('sendUpdate("rad_companions", true);') == 2,
      "the local layout editor must default, show, save and reset rad_companions")
check("var WIDGET_ID='rad_companions';" in widget_html and "window.setLocalHUDLayout" in widget_html and "window.releaseLocalHUDLayout" in widget_html
      and "window.setEditorPreview" in widget_html and "window.setHUDPosition" in widget_html and "window.updateCompanions" in widget_html,
      "the companion widget must speak the layout bridge and the editor preview protocol")
check("var DEF={xf:22/1920,yf:530/1080,wf:360/1920,hf:98/1080};" in widget_html, "the widget default matches the own-vitals slot")
check("var STAGE_COLORS=['var(--muted)','var(--amber)','var(--rad)','var(--rad)','var(--cyan)'];" in widget_html
      and "var HEALTHY_COLOR='var(--muted)';" in widget_html and "var STAGE_FILL=[.25,.5,.75,1,1];" in widget_html,
      "companion stage map: muted 5, amber 10, rad 15, rad 20 plus crit, cyan full ring")
check("PhosCore.build(" in widget_html and 'href="hud_status.css"' in widget_html and 'src="hud_core.js"' in widget_html,
      "the companion widget must use the shared Phosphor cores")
check("e.hidden=s<0&&!always;" in widget_html, "healthy companions show only under the always-show tuning key")
needs_html = text(VIEW / "hud_needs.html")
illness_icon = re.search(r"PHOS_ICONS\.illness=\['(<path.*?)','ILLNESS'\];", needs_html)
check(illness_icon is not None and "var ILLNESS_ICON='" + illness_icon.group(1) + "';" in widget_html,
      "the player core must use the same illness icon path as hud_needs.html")
check("PhosCore.build(document.getElementById('c0Core'),ILLNESS_ICON,'YOU')" in widget_html
      and "PhosCore.build(document.getElementById('c'+i+'Core'),ICON,'HEALTHY')" in widget_html,
      "the player is the first core, built with PhosCore and the illness icon; companions keep the person icon")
names_line = re.search(r"var DISEASE_NAMES=\{[^}]*\};", needs_html)
check(names_line is not None and names_line.group(0) in widget_html and "7:'WOUND INFECTION'" in names_line.group(0)
      and "Math.min(73," in widget_html and "id===7&&stage>=3?'SEPSIS'" in widget_html,
      "the player core names every illness like the needs widget, up to id 7 with SEPSIS at wound stage 3")
check("e.hidden=id===0&&!always;" in widget_html and "var show=any||_preview;" in widget_html
      and "any=renderYou(d,always)" in widget_html,
      "a healthy player is hidden unless always-show is on, and a sick player alone makes the widget visible")
check("s=Math.min(stage,3)" in widget_html and "id!==0&&s===3?'crit':''" in widget_html and "onset" not in widget_html,
      "player stage mapping follows the shared stage map, with crit at stage 3 and no separate onset arc")
check("s===3?'crit':''" in widget_html and "PREVIEW=[" in widget_html and "previewTick" in widget_html and "setInterval" not in widget_html
      and "{n:'Hancock',s:4}" in widget_html and "you:{d:23,o:0}" in widget_html and "{n:'Dogmeat',s:0}" in widget_html,
      "companions at stage 3 take crit and the editor preview cycles every state without setInterval")
check('R"(,"you":{"d":)" + std::to_string(_youDisease)' in widget_cpp and "_youDisease = snapshot.diseasePacked;" in text(SRC / "RadHudNeeds.cpp")
      and "_youOnset" in text(SRC / "RadHudNeeds.cpp"),
      "the player illness reaches the widget from the NeedsRuntime snapshot (diseasePacked, diseaseOnset01)")
check("_youDisease = 0;" in hud_main and '{ "you", json{ { "d", hud.YouDisease() }, { "o", hud.YouOnset() } } }' in runtime,
      "the player row resets with the needs widget and is mirrored in the debug payload")

check('id="tabbtn-outbreak"' in debug_html and "onclick=\"showTab('outbreak')\"" in debug_html and 'id="tab-outbreak"' in debug_html,
      "the debug menu needs an OUTBREAK tab")
pane = debug_html.split('id="tab-outbreak"', 1)[1].split('id="tab-hud"', 1)[0] if 'id="tab-outbreak"' in debug_html else ""
survival_pane = debug_html.split('id="tab-survival"', 1)[1].split('id="tab-showcase"', 1)[0] if 'id="tab-survival"' in debug_html else ""
for number in (1, 2, 3, 4):
    check(f"cmd('outbreak:{number}')" in pane and f"cmd('outbreak:{number}')" not in survival_pane,
          f"outbreak:{number} must live on the OUTBREAK tab, not the survival tab")
check('id="outbreakCompanions"' in pane and 'id="outbreakNearby"' in pane, "the OUTBREAK tab lists companions and nearby counts")
check('id="outbreakDetector"' in pane and 'getElementById("outbreakDetector")' in debug_js and "data.scanner" in debug_js and "the result shows as a notification" not in pane,
      "the OUTBREAK tab shows the scanner flag, and its help no longer claims every command posts a notification")
check(debug_js.count('"hud", "outbreak", "injury"]') == 2 and 'e.key <= "9"' in debug_js, "both tab lists and the number-key range must include the OUTBREAK and INJURY tabs")
check("window.updateOutbreakStatus" in debug_js and "OUTBREAK_STAGES" in debug_js, "the debug view must expose updateOutbreakStatus")
check('obRow("You"' in debug_js and "data.you" in debug_js, "the debug tab mirrors the player row first")
check('"Incubating", "Early", "Progress", "Final", "Recovered"' in debug_js, "the debug tab names the five stages")
check('obEl("ol", "ob-track")' in debug_js and '"cur"' in debug_js and '"past"' in debug_js and "innerHTML" not in outbreak_js,
      "each companion gets a five-step track built with DOM calls: current lit, past dim-lit")
check('"No sick NPCs within 4096 units."' in debug_js and 'obEl("div", "ob-bar")' in debug_js and "seg.style.flexGrow = n[j];" in debug_js and 'obEl("b", n[j] ? "" : "zero"' in debug_js,
      "the nearby card is one segmented bar with counts per stage and an empty-state message")
check('"Unknown (RadOutbreak.esp missing)"' in debug_js and '"Healthy"' in debug_js, "the missing-plugin and healthy states stay")
check('<dl id="outbreak' not in pane and 'class="diag-card"' in pane and 'class="pip-sec-head"' in pane and 'class="diagnostic-grid"' in pane,
      "the OUTBREAK tab keeps the debug menu card structure and uses div containers for the live rows")
debug_css = text(VIEW / "radhud.css")
ob_css = debug_css.split("#tab-outbreak {", 1)[1].split(".pip-mini-meter {", 1)[0] if "#tab-outbreak {" in debug_css else ""
check(ob_css != "" and all(token in ob_css for token in ("--ob-muted: #8fa996;", "--ob-amber: #f2c46a;", "--ob-rad: #ff7052;", "--ob-cyan: #86e6ea;")),
      "the debug stage colours are the PhosCore tokens")
check(all(selector in ob_css for selector in ("#tab-outbreak .ob-track li.cur", "#tab-outbreak .ob-track li.past", "#tab-outbreak .ob-bar span", "#tab-outbreak .ob-counts b")),
      "the track, bar and count rules exist")
check(all(line.startswith("#tab-outbreak") for line in ob_css.splitlines() if line.endswith(("{", ","))),
      "every selector in the outbreak block is scoped under #tab-outbreak")
check("var(--pip-border-dim)" in ob_css and "var(--pip-font-mono)" in ob_css and "tabular-nums" in ob_css,
      "the debug rules use the radhud.css variables and tabular numbers")

if failures:
    print("outbreak detector contract failures:")
    for failure in failures:
        print(f"  FAIL {failure}")
    sys.exit(1)

print("Outbreak detector verified: runtime scan, companion widget, debug tab, overlay view and record IDs")
