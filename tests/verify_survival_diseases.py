"""Record contract for the taint shortlists, the carried cure, the seven illnesses and the cold ladder.

`tests/verify_survival.py` already owns the disease rules and the script wiring, and it sits at the
repository's 750-line cap, so the record-level half lives here: the manifest in
`tools/rad_diseases.json` is read, `deploy/RadSurvival.esp` is parsed with `tests/plugin_reader.py`,
and every keyword, form list, ingestible, magic effect and VMAD binding the script depends on is
checked against it. Read-only; re-authoring happens through FO4IDE.
"""

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "deploy/RadSurvival.esp"
MANIFEST = ROOT / "tools/rad_diseases.json"
DISEASE_SOURCE = ROOT / "deploy/Scripts/Source/User/RadSurvivalDiseases.psc"
QUEST_SOURCE = ROOT / "deploy/Scripts/Source/User/RadSurvivalQuest.psc"
NATIVES_SOURCE = ROOT / "deploy/Scripts/Source/User/RadNatives.psc"
SHOWCASE_SOURCE = ROOT / "deploy/Scripts/Source/User/RadShowcase.psc"
NEEDS_POLICY = ROOT / "src/NeedsPolicy.h"
CLIMATE_POLICY = ROOT / "src/ClimatePolicy.h"
NAMES_PATH = ROOT / "tools/rad_names.json"
RAD_AWAY_ID = 0x023742

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plugin_reader  # noqa: E402

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def text(path):
    return path.read_text(encoding="utf-8")


def function_body(source_code, signature):
    match = re.search(re.escape(signature) + r"(.*?)\nEndFunction", source_code, re.S)
    return match.group(1) if match else ""


def psc_is(source_code, name, expected):
    match = re.search(rf"(?:int|float) Property {name} = (-?[0-9.]+) Auto", source_code)
    return match is not None and abs(float(match.group(1)) - float(expected)) < 1e-6


def local_id(key):
    return int(key.split(":")[0], 16)


def per_disease(source_code, signature):
    return {int(n): float(v) for n, v in re.findall(r"aiDisease == (\d+)\s+return ([0-9.]+)",
                                                    function_body(source_code, signature))}


def spell_effects(esp_bytes, node):
    effects = []
    for sig, start, size in node.subrecords(esp_bytes):
        if sig == b"EFID":
            effects.append([int.from_bytes(esp_bytes[start:start + 4], "little"), None])
        elif sig == b"EFIT" and effects:
            effects[-1][1] = struct.unpack("<fII", esp_bytes[start:start + 12])
    return effects


def full_name(esp_bytes, node):
    raw = node.field(esp_bytes, b"FULL")
    return raw.rstrip(b"\x00").decode("utf-8") if raw is not None else None


check(PLUGIN.is_file(), f"missing {PLUGIN.relative_to(ROOT)}")
check(MANIFEST.is_file(), f"missing {MANIFEST.relative_to(ROOT)}")
check(DISEASE_SOURCE.is_file(), f"missing {DISEASE_SOURCE.relative_to(ROOT)}")
diseases = json.loads(text(MANIFEST)) if MANIFEST.is_file() else {}
disease_code = DISEASE_SOURCE.read_text(encoding="utf-8") if DISEASE_SOURCE.is_file() else ""

quest_code = text(QUEST_SOURCE) if QUEST_SOURCE.is_file() else ""
natives_code = text(NATIVES_SOURCE) if NATIVES_SOURCE.is_file() else ""
showcase_code = text(SHOWCASE_SOURCE) if SHOWCASE_SOURCE.is_file() else ""
rad_names = json.loads(text(NAMES_PATH)).get("names", {}) if NAMES_PATH.is_file() else {}
layer = diseases.get("diseases", [])
max_id = diseases.get("diseaseMaxId")
check(max_id == 7 and [entry.get("id") for entry in layer] == list(range(1, 8)),
      "the manifest must declare seven diseases with ids 1-7")
check(psc_is(disease_code, "MaxDiseaseId", max_id or 0),
      "MaxDiseaseId in the disease script diverged from the manifest")
policy_max = re.search(r"kDiseaseMaxId = (\d+);", text(NEEDS_POLICY)) if NEEDS_POLICY.is_file() else None
check(policy_max is not None and int(policy_max.group(1)) == max_id,
      "src/NeedsPolicy.h kDiseaseMaxId must equal the manifest's disease count, or the HUD drops ids above it")
for guarded in ("Function DebugSetDisease(int aiDisease, int aiStage, bool abActive)", "Function Resync(bool abActive)"):
    check("MaxDiseaseId" in function_body(disease_code, guarded), f"{guarded} must bound the id by MaxDiseaseId")

bases = diseases.get("illnessBases", {})
first, stride = bases.get("firstDisease", 0), bases.get("stride", 0)
for prop_key, base_key in (("spellProperty", "spellBase"), ("perkProperty", "perkBase")):
    check(psc_is(disease_code, bases.get(prop_key, "-"), bases.get(base_key, -1)),
          f"{bases.get(prop_key)} diverged from the manifest {base_key}")
for kind in ("Spell", "Perk"):
    selector = function_body(disease_code, f"{kind} Function Disease{kind}For(int aiDisease, int aiStage)")
    check(f"Illness{kind}Base + (aiDisease - {first}) * {stride}" in selector and "aiDisease <= MaxDiseaseId" in selector,
          f"Disease{kind}For must resolve diseases {first}-{max_id} from Illness{kind}Base in strides of {stride}")
label_body = function_body(disease_code, "string Function DiseaseLabel(int aiDisease)")
progress_body = function_body(disease_code, "float Function ProgressForStage(int aiDisease, int aiStage)")
check("IncubationFor(aiDisease)" in progress_body and "Stage2For(aiDisease)" in progress_body
      and "Stage3For(aiDisease)" in progress_body, "ProgressForStage must read the per-disease timings")
check("ProgressForStage(aiDisease, DiseaseStage)" in function_body(
    disease_code, "Function DebugSetDisease(int aiDisease, int aiStage, bool abActive)"),
      "a debug stage must seed progress from the per-disease timings")
timing_functions = {"incubationHours": "IncubationFor", "stage2Hours": "Stage2For", "stage3Hours": "Stage3For"}
timing_tables = {key: per_disease(disease_code, f"float Function {name}(int aiDisease)")
                 for key, name in timing_functions.items()}
for disease in layer:
    did = disease.get("id", 0)
    name = disease.get("name", "")
    perk_base = int(disease.get("perkBase", 0))
    check(f'return "{name}"' in label_body, f"DiseaseLabel must name {name}")
    if did >= first:
        offset = (did - first) * stride
        check(disease.get("spellBase") == bases.get("spellBase", 0) + offset
              and disease.get("perkBase") == bases.get("perkBase", 0) + offset,
              f"{name} spell/perk bases must follow the illness stride")
        timings = disease.get("timings", {})
        for key, table in timing_tables.items():
            check(timings.get(key) is not None and table.get(did) == float(timings[key]),
                  f"{name} {key} diverged from {timing_functions[key]} in the disease script")
        check(0 < timings.get("incubationHours", 0) < timings.get("stage2Hours", 0) < timings.get("stage3Hours", 0),
              f"{name} timings must grow stage by stage")
    else:
        check(all(did not in table for table in timing_tables.values()) and "timings" not in disease,
              f"{name} must keep the tuned default timings")
        check(psc_is(disease_code, f"{name.capitalize()}PerkBase", perk_base)
              and psc_is(disease_code, f"{name.capitalize()}SpellBase", disease.get("spellBase", -1)),
              f"{name} bases diverged from the disease script")
    for perk in disease.get("perks", []):
        check(local_id(perk.get("recordFormId", "0x0")) == (perk_base + perk.get("stage", 0) - 1) | 0x01000000
              and rad_names.get(perk.get("editorId")) == perk.get("name"),
              f"{perk.get('editorId')} must sit at its perk base and carry its tools/rad_names.json name")
    check(len(disease.get("perks", [])) == 3, f"{name} needs one perk per stage")
sepsis = next((entry for entry in layer if entry.get("id") == 7), {})
check(sepsis.get("stage3Label") == "sepsis" and "DiseaseStage >= 3" in label_body and 'return "sepsis"' in label_body,
      "wound infection must read as sepsis at stage 3")

causes = diseases.get("causes", {})
cause_signatures = {
    "ExposureConsume": "Function ExposureConsume(Form akItem, int aiCount, bool abActive)",
    "RollWound": "Function RollWound(Form akSource, Actor akPlayer, bool abActive)",
    "RollCold": "Function RollCold(float afHours, bool abActive)",
}
check(len(causes.get("entries", [])) == 5, "the manifest must list five disease causes")
for cause in causes.get("entries", []):
    body = function_body(disease_code, cause_signatures.get(cause.get("function"), "-"))
    prop = cause.get("chanceProperty", "-")
    check(f"RollCause({cause.get('disease')}," in body and prop in body,
          f"{cause.get('function')} must roll disease {cause.get('disease')} with {prop}")
    check(psc_is(disease_code, prop, cause.get("chance", -1)), f"{prop} diverged from the manifest")
check(psc_is(disease_code, causes.get("woundHealthFractionProperty", "-"), causes.get("woundHealthFraction", -1)),
      "WoundHealthFraction diverged from the manifest")
check(psc_is(disease_code, causes.get("foodPoisoningClearProperty", "-"), causes.get("foodPoisoningClearHours", -1)),
      "FoodPoisoningClearHours diverged from the manifest")
food_poisoning = next((entry for entry in layer if entry.get("id") == 4), {})
check(causes.get("foodPoisoningClearHours", 0) > food_poisoning.get("timings", {}).get("stage3Hours", 1e9),
      "food poisoning must run through its stages before it clears itself")
advance_text = function_body(disease_code, "Function AdvanceDisease(float afHours, bool abActive)")
check("DiseaseId == 4 && DiseaseProgress >= FoodPoisoningClearHours" in advance_text and "CureDisease()" in advance_text,
      "food poisoning must clear itself once its clear time passes")
roll_cause = function_body(disease_code, "Function RollCause(int aiDisease, float afChance, bool abActive)")
for guard in ("!abActive", "!DiseasesOn()", "DiseaseId > 0", "IsImmune()", "StartSpecific(aiDisease, abActive)"):
    check(guard in roll_cause, f"RollCause must be gated by {guard}")
check("Roll(afChance)" in roll_cause, "a cause must go through the scaled roll")
cause_scale = function_body(disease_code, "float Function CauseScale()")
check("CauseScale()" in function_body(disease_code, "bool Function Roll(float afChance)"),
      "every cause roll must be scaled by DiseaseCauseScale")
check(f'RadNatives.TuningByName("{causes.get("scaleTuningKey")}")' in cause_scale and "raw - 1.0" in cause_scale,
      "CauseScale must read the manifest's tuning key and undo the native's +1 set marker")
specific = function_body(disease_code, "Function StartSpecific(int aiDisease, bool abActive)")
check("DiseaseId = aiDisease" in specific and "DiseaseStage = 0" in specific and "DiseaseProgress = 0.0" in specific
      and "MirrorDisease()" in specific and "Reconcile(abActive)" in specific,
      "StartSpecific must start the named disease incubating and refresh the mirrors")
wound = function_body(disease_code, "Function RollWound(Form akSource, Actor akPlayer, bool abActive)")
check("IsMeleeWeapon(akSource)" in wound and "GetValuePercentage(Game.GetHealthAV()) < WoundHealthFraction" in wound,
      "RollWound must key tetanus off melee and wound infection off the health fraction")
cold_roll = function_body(disease_code, "Function RollCold(float afHours, bool abActive)")
check("ColdStage < 2" in cold_roll and "((ColdStage - 1) as float)" in cold_roll and "afHours" in cold_roll,
      "pneumonia must roll from cold stage 2 up, scaled by stage and hours")
check("RollCold(afHours, abActive)" in function_body(
    disease_code, "Function Tick(float afHours, bool abActive, int aiHungerStage, int aiThirstStage, int aiSleepStage)"),
      "the disease tick must roll the cold cause")
melee = function_body(disease_code, "bool Function IsMeleeWeapon(Form akSource)")
check("akSource as Weapon" in melee, "melee detection must reject non-weapons first")
for keyword in causes.get("meleeKeywords", []):
    check(psc_is(disease_code, keyword.get("property", "-"), local_id(keyword.get("recordFormId", "0x0")))
          and f'GetFormFromFile({keyword.get("property")}, "Fallout4.esm")' in melee,
          f"{keyword.get('editorId')} must resolve from Fallout4.esm by {keyword.get('property')}")

treatments = diseases.get("treatments", {})
outright_text = function_body(disease_code, "bool Function CuresOutright(Form akItem)")
treats_text = function_body(disease_code, "bool Function TreatsDisease(Form akItem, int aiDisease)")
treated = {}
for condition, prop in re.findall(r"if ((?:aiDisease == \d+(?: \|\| )?)+)\s+return IsFormFrom\(akItem, (\w+),", treats_text):
    treated[prop] = {int(number) for number in re.findall(r"aiDisease == (\d+)", condition)}
check(len(treatments.get("stepDown", [])) == 3 and len(treatments.get("cureOutright", [])) == 2,
      "the manifest must list three step-down items and two outright cures")
for item in treatments.get("stepDown", []) + treatments.get("cureOutright", []):
    check(item.get("recordFormId", "").endswith(":Fallout4.esm")
          and psc_is(disease_code, item.get("property", "-"), local_id(item.get("recordFormId", "0x0"))),
          f"{item.get('name')} must resolve from Fallout4.esm by {item.get('property')}")
for item in treatments.get("stepDown", []):
    check(treated.get(item.get("property")) == set(item.get("diseases", [])),
          f"TreatsDisease must map {item.get('name')} to diseases {item.get('diseases')}, found {treated.get(item.get('property'))}")
    check(item.get("property") not in outright_text, f"{item.get('name')} must step a stage, not cure outright")
for item in treatments.get("cureOutright", []):
    check(item.get("property") in outright_text, f"CuresOutright must accept {item.get('name')}")
radaway = next((item for item in treatments.get("cureOutright", []) if item.get("name") == "RadAway"), {})
check(radaway.get("diseases") == [1, 2] and "return DiseaseId <= 2" in outright_text,
      "RadAway must cure only fever and wasting")
check(RAD_AWAY_ID == local_id(radaway.get("recordFormId", "0x0")), "RadAway must stay the manifest's obtainable cure")
step = function_body(disease_code, "Function TreatStep(bool abActive)")
check("DiseaseStage <= 1" in step and "CureDisease()" in step and "DiseaseStage -= 1" in step
      and "ProgressForStage(DiseaseId, DiseaseStage)" in step and "Reconcile(abActive)" in step and "MirrorDisease()" in step,
      "TreatStep must cure at the first stage and otherwise step down, re-seed progress and re-apply the ability")
rest = function_body(disease_code, "Function TryRestCure(float afHours, bool abInterrupted, bool abActive)")
rest_rules = treatments.get("restCure", {})
check(f"DiseaseId == 5 && DiseaseStage > {rest_rules.get('tetanusBlockedFromStage', 0) - 1}" in rest
      and "DiseaseId == 6 && ColdStage > 0" in rest and rest_rules.get("pneumoniaBlockedWhileCold") is True,
      "rest must not cure advanced tetanus or pneumonia while cold")
lethal = diseases.get("lethal", {})
for key in ("delay", "terminal", "warn", "drain"):
    check(psc_is(disease_code, lethal.get(f"{key}Property", "-"), lethal.get(f"{key}Hours", lethal.get(f"{key}PerHour", -1))),
          f"{lethal.get(f'{key}Property')} diverged from the manifest")
check("AdvanceTerminal(afHours)" in advance_text, "AdvanceDisease must run the terminal phase")
terminal = function_body(disease_code, "Function AdvanceTerminal(float afHours)")
check("DiseaseStage < 3" in terminal and "DiseaseId == 4" in terminal and "!LethalOn()" in terminal
      and "TerminalPhase = 0" in terminal, "the terminal phase must stop below stage 3, for food poisoning and when switched off")
check(terminal.count("Debug.Notification(") == 3 and "NotifyDisease(" not in terminal and "TerminalPhase = 1" in terminal and "TerminalPhase = 2" in terminal
      and "DiseaseProgress = start" in terminal and "DiseaseProgress = warn" in terminal,
      "the terminal phase must warn at its start and near its end, and a time jump must stop at each warning")
check("DiseaseProgress - afHours < finish" in terminal and "DiseaseProgress = finish" in terminal
      and "elseif TerminalPhase >= 2 && DiseaseProgress >= finish" in terminal and "player.Kill()" in terminal,
      "an untreated illness must kill only after the second warning, and never inside the jump that crosses the end")
god = terminal.find("if RadNatives.IsGodMode()")
check(0 <= terminal.find("TerminalPhase = 2") < god < terminal.find("player.Kill()") < terminal.find("player.DamageValue(")
      and terminal.count("DiseaseProgress = warn") == 2,
      "god mode must stop the drain and the kill after the warnings and hold progress at the second warning")
check("player.GetValue(health) - 1.0" in terminal and "player.DamageValue(health, drain)" in terminal,
      "the terminal drain must never take the last point of health")
check(f'TunedByName("{lethal.get("enabledTuningKey")}", 1.0) >= 0.5' in function_body(disease_code, "bool Function LethalOn()")
      and "DiseasesOn()" in function_body(disease_code, "bool Function LethalOn()"),
      "LethalOn must read the off switch and follow DiseasesOn")
check(f'TunedByName("{lethal.get("delayTuningKey")}", {lethal.get("delayProperty")})' in function_body(disease_code, "float Function LethalDelay()"),
      "LethalDelay must read the hours setting over the script default")
for signature in ("Function CureDisease()", "Function TreatStep(bool abActive)",
                  "Function DebugSetDisease(int aiDisease, int aiStage, bool abActive)"):
    check("TerminalPhase = 0" in function_body(disease_code, signature), f"{signature} must end the terminal phase")
check(lethal.get("exempt") == [4], "only food poisoning is exempt from dying")
sleep = quest_code.split("Event OnPlayerSleepStop(", 1)[-1].split("EndEvent", 1)[0]
check(0 <= sleep.find("RestCureDiseases(") < sleep.find("AccrueElapsed("),
      "sleep must try the rest cure before the slept hours advance the illness")
thirst = treatments.get("thirstScale", {})
thirst_text = function_body(disease_code, "float Function ThirstScale()")
check(f"DiseaseId != {thirst.get('disease')}" in thirst_text
      and re.findall(r"return ([0-9.]+)", thirst_text) == ["1.0"] + [str(float(v)) for v in thirst.get("stages", [])],
      "ThirstScale must scale dysentery thirst by the manifest stages and leave everything else at 1.0")
check("DiseaseThirstScale()" in function_body(quest_code, "Function Accrue(float afHours)")
      and "Diseases.ThirstScale()" in function_body(quest_code, "float Function DiseaseThirstScale()"),
      "the thirst pool must accrue through the disease thirst scale")

cold_manifest = diseases.get("cold", {})
check(psc_is(quest_code, cold_manifest.get("spellBaseProperty", "-"), cold_manifest.get("spellBase", -1))
      and psc_is(quest_code, cold_manifest.get("perkBaseProperty", "-"), cold_manifest.get("perkBase", -1)),
      "ColdSpellBase/ColdPerkBase diverged from the manifest")
hot_food = cold_manifest.get("hotFood", {})
check(psc_is(quest_code, hot_food.get("property", "-"), hot_food.get("formIdDecimal", -1))
      and local_id(hot_food.get("recordFormId", "0x0")) & 0xFFFFFF == hot_food.get("formIdDecimal")
      and psc_is(quest_code, hot_food.get("warmHoursProperty", "-"), hot_food.get("warmHours", -1)),
      "the hot-food list id and warm hours diverged from the quest script")
apply_cold = function_body(quest_code, "Function ApplyColdStage(int aiStage)")
for needle in ("aiStage < 0 || aiStage > 5 || !IsActive()", "Diseases.SetColdStage(aiStage)",
               "ColdSpellBase + aiStage - 1", "ColdPerkBase + aiStage - 1", "AddSpell(", "RemoveSpell(", "AddPerk(", "RemovePerk("):
    check(needle in apply_cold, f"ApplyColdStage must contain {needle}")
warm = function_body(quest_code, "Function WarmFromFood(Form akItem, int aiCount)")
check("HotFoodListFormId" in warm and "hot.HasForm(akItem)" in warm
      and "RadNatives.WarmBody(HotFoodWarmHours * (aiCount as float))" in warm and "!IsActive()" in warm,
      "WarmFromFood must warm the body only for an active layer and a hot-food list entry")
relief_text = function_body(quest_code, "Function ApplyRelief(Form akItem, int aiCount)")
check("WarmFromFood(akItem, aiCount)" in relief_text
      and relief_text.index("Diseases.ConsumeItem(") < relief_text.index("WarmFromFood("),
      "a confirmed consumption must reach the hot-food warmth after the disease layer")
check("float Function TuningByName(string asKey) global native" in natives_code
      and "Function WarmBody(float afHours) global native" in natives_code,
      "RadNatives must declare the tuning-by-name and warm-body natives")
check("Function ApplyCold(int aiStage) Global" in showcase_code and "quest.ApplyColdStage(aiStage)" in showcase_code,
      "RadShowcase.ApplyCold must forward the stage to the quest")
policy_rows = (re.findall(r"case HypothermiaStage::k\w+:\s+return \{ ([^}]+) \};", text(CLIMATE_POLICY))
               if CLIMATE_POLICY.is_file() else [])
check(len(policy_rows) == 5 == len(cold_manifest.get("stages", [])),
      "ClimatePolicy EffectsForStage must define the five cold stages the manifest lists")
for row, cold_stage in zip(policy_rows, cold_manifest.get("stages", [])):
    numbers = [float(value) for value in re.findall(r"-?[0-9.]+", row)]
    cold_effects = cold_stage.get("effects", {})
    expected = {"speed": round((numbers[0] - 1.0) * 100), "apRate": round((numbers[1] - 1.0) * 100),
                "reload": round(numbers[2] - 1.0, 2), "aim": round(numbers[2] - 1.0, 2), "strength": numbers[4],
                "endurance": numbers[5], "agility": numbers[6]}
    for key, value in expected.items():
        check(abs(cold_effects.get(key, 0) - value) < 1e-6,
              f"cold stage {cold_stage.get('stage')} {key} {cold_effects.get(key, 0)} must follow ClimatePolicy ({value})")
for cold_stage in cold_manifest.get("stages", []):
    check(rad_names.get(cold_stage.get("perkEditorId")) == cold_stage.get("perkName")
          and local_id(cold_stage.get("perkFormId", "0x0")) == (cold_manifest.get("perkBase", 0) + cold_stage.get("stage", 0) - 1) | 0x01000000,
          f"{cold_stage.get('perkEditorId')} must sit at ColdPerkBase and carry its tools/rad_names.json name")
for script, names in (
    ("RadSurvivalDiseases", ["RollCause", "RollWound", "RollCold", "TreatStep", "CuresOutright", "TreatsDisease",
                             "ThirstScale", "StartSpecific", "IncubationFor", "Stage2For", "Stage3For", "SetColdStage",
                             "IsMeleeWeapon", "CauseScale", "ProgressForStage", "IllnessSpellBase", "IllnessPerkBase",
                             "MaxDiseaseId", "DiseaseCauseScale", "sepsis", "AdvanceTerminal", "DiseaseLethal",
                             "DiseaseLethalHours", "TerminalPhase", "IsGodMode"]
     + [cause["chanceProperty"] for cause in causes.get("entries", [])]),
    ("RadSurvivalQuest", ["ApplyColdStage", "WarmFromFood", "DiseaseThirstScale", "ColdSpellBase", "ColdPerkBase",
                         "HotFoodListFormId"]),
    ("RadNatives", ["TuningByName", "WarmBody", "IsGodMode"]),
    ("RadShowcase", ["ApplyCold", "ApplyColdStage"]),
):
    pex_path = ROOT / "deploy/Scripts" / f"{script}.pex"
    pex_bytes = pex_path.read_bytes() if pex_path.is_file() else b""
    for name in names:
        check(name.encode() in pex_bytes, f"{script}.pex was not recompiled from the source: missing {name}")

if PLUGIN.is_file():
    esp = PLUGIN.read_bytes()
    check(esp[:4] == b"TES4", "RadSurvival.esp is not a plugin")
    try:
        tree = plugin_reader.parse(esp)
    except plugin_reader.PluginError as exc:
        tree = []
        check(False, f"RadSurvival.esp does not parse as a plugin: {exc}")

    def plugin_records(signature):
        return plugin_reader.records_by_form_id(esp, tree, signature)

    taint = diseases.get("taint", {})
    taint_keywords = taint.get("keywords", [])
    taint_lists = taint.get("lists", [])
    cure = taint.get("cure", {})
    check(len(taint_keywords) == 4,
          "the taint table must declare three exposure keywords and the cure marker")
    check(len(taint_lists) == 3 and sorted(entry.get("category", 0) for entry in taint_lists) == [1, 2, 3],
          "the taint table must name one list per exposure category, 1-3 exactly once")

    keyword_ids = {}
    for entry in taint_keywords:
        form_id = int(entry.get("formKey", "0x0"), 16)
        keyword_ids[entry.get("editorId")] = form_id
        node = plugin_records(b"KYWD").get(form_id)
        check(node is not None and plugin_reader.editor_id(esp, node) == entry.get("editorId"),
              f"missing taint keyword {entry.get('editorId')}")

    for entry in taint_lists:
        node = plugin_records(b"FLST").get(int(entry.get("formKey", "0x0"), 16))
        check(node is not None and plugin_reader.editor_id(esp, node) == entry.get("editorId"),
              f"missing taint form list {entry.get('editorId')}")
        if node is None:
            continue
        expected = [int(key.split(":")[0], 16) for key in entry.get("entries", [])]
        actual = [int.from_bytes(esp[start:start + 4], "little")
                  for sig, start, size in node.subrecords(esp) if sig == b"LNAM"]
        check(expected and actual == expected,
              f"{entry.get('editorId')} entries diverged from the manifest")
        check(entry.get("keyword") in keyword_ids,
              f"{entry.get('editorId')} names no authored keyword")
        check(f"FormList Property {entry.get('property')} Auto" in disease_code,
              f"the disease script must read {entry.get('property')}")

    item = plugin_records(b"ALCH").get(int(cure.get("itemFormId", "0x0"), 16))
    effect = plugin_records(b"MGEF").get(int(cure.get("effectFormId", "0x0"), 16))
    check(item is not None and effect is not None, "missing the cure ingestible or its effect")
    if item is not None and effect is not None:
        check(plugin_reader.editor_id(esp, item) == cure.get("itemEditorId")
              and plugin_reader.editor_id(esp, effect) == cure.get("effectEditorId"),
              "the cure ingestible and its effect must be the records the manifest names")
        efid = item.field(esp, b"EFID") or b"\x00" * 4
        enit = item.field(esp, b"ENIT") or b"\x00" * 20
        weight = item.field(esp, b"DATA") or b"\x00" * 4
        data = effect.field(esp, b"DATA") or b""
        check(int.from_bytes(efid, "little") == int(cure.get("effectFormId", "0x0"), 16),
              "the cure item must reference the authored effect, or consuming it does nothing")
        check(int.from_bytes(enit[:4], "little") == int(cure.get("itemValue", 0))
              and abs(struct.unpack("<f", weight[:4])[0] - float(cure.get("itemWeight", 0))) < 1e-6,
              "the cure item value or weight does not match the manifest")
        check(len(data) >= 68
              and int.from_bytes(data[0:4], "little") == int(cure.get("effectFlags", 0))
              and int.from_bytes(data[64:68], "little") == int(cure.get("effectArchetype", 0)),
              "the cure effect must keep the manifest's flags and Script archetype")

    recipe = cure.get("recipe", {})
    cobj = plugin_records(b"COBJ").get(int(recipe.get("recordFormId", "0x0"), 16))
    check(cobj is not None and plugin_reader.editor_id(esp, cobj) == recipe.get("editorId"), "the cure recipe is missing")
    if cobj is not None:
        def form(key):
            return int(key.split(":")[0].split()[0], 16)
        def u32(sig):
            raw = cobj.field(esp, sig)
            return int.from_bytes(raw, "little") if raw else -1
        check(u32(b"CNAM") == int(cure.get("itemFormId", "0x0"), 16), "the recipe must create the cure item")
        check(u32(b"BNAM") == form(recipe.get("workbenchKeyword", "0")), "the recipe must be made at the Chemistry Station")
        check(u32(b"FNAM") == form(recipe.get("category", "0")), "the recipe must sit in the healing category")
        check(u32(b"INTV") == recipe.get("createdCount"), "the recipe must make one item")
        fvpa = cobj.field(esp, b"FVPA") or b""
        parts = [struct.unpack_from("<II", fvpa, i) for i in range(0, len(fvpa), 8)]
        check(parts == [(form(c["recordFormId"]), c["count"]) for c in recipe.get("components", [])],
              f"the recipe components diverged from the manifest: {parts}")
        ctda = [esp[start:start + size] for sig, start, size in cobj.subrecords(esp) if sig == b"CTDA"]
        perks = [(int.from_bytes(c[12:16], "little"), struct.unpack_from("<f", c, 4)[0], c[0]) for c in ctda]
        wanted = recipe.get("anyPerk", [])
        check(len(perks) == len(wanted) and all(p[0] == form(w["recordFormId"]) and p[1] == w["rank"] and int.from_bytes(c[8:10], "little") == 448
                                                 for p, w, c in zip(perks, wanted, ctda))
              and all(p[2] & 1 for p in perks[:-1]) and all(p[2] & 0xE0 == 0x60 for p in perks),
              "the recipe must need any one of the manifest perks (HasPerk >= rank, OR-joined)")
    quest = plugin_records(b"QUST").get(0x01000800)
    scripts = plugin_reader.vmad_properties(esp, quest) if quest is not None else {}
    check("RadSurvivalDiseases" in scripts, "the quest VMAD must attach the disease script")
    disease_script = scripts.get("RadSurvivalDiseases", {})
    for entry in taint_lists + taint_keywords:
        name = entry.get("property")
        form_id = int(entry.get("formKey", "0x0"), 16)
        check(disease_script.get(name) == form_id,
              f"the disease script must bind {name} to 0x{form_id:08X}, found {disease_script.get(name)}")

    all_mgefs = plugin_records(b"MGEF")
    all_spells = plugin_records(b"SPEL")
    all_perks = plugin_records(b"PERK")
    all_lists = plugin_records(b"FLST")
    expected_perks = [perk for entry in layer for perk in entry.get("perks", [])]
    expected_perks += [{"recordFormId": stage.get("perkFormId"), "editorId": stage.get("perkEditorId"), "name": stage.get("perkName")}
                       for stage in cold_manifest.get("stages", [])]
    check(len(all_perks) == len(expected_perks) == 26,
          f"RadSurvival.esp must hold 26 perks (21 disease + 5 cold), found {len(all_perks)}")
    for perk in expected_perks:
        node = all_perks.get(int(perk.get("recordFormId", "0x0"), 16))
        check(node is not None and plugin_reader.editor_id(esp, node) == perk.get("editorId"),
              f"missing perk {perk.get('editorId')}")
        if node is not None:
            check(full_name(esp, node) == perk.get("name"),
                  f"{perk.get('editorId')} must be named {perk.get('name')!r}, found {full_name(esp, node)!r}")

    for entry in layer:
        for stage in entry.get("stages", []):
            spell_node = all_spells.get(int(stage.get("spellFormId", "0x0"), 16))
            if spell_node is None:
                continue
            effects = spell_effects(esp, spell_node)
            check(len(effects) == 1 and effects[0][0] == int(stage.get("mgefFormId", "0x0"), 16),
                  f"{stage.get('spellEditorId')} must carry exactly its own magic effect")

    cold_mgef_ids = {}
    for mgef in cold_manifest.get("mgefs", []):
        form_id = int(mgef.get("recordFormId", "0x0"), 16)
        cold_mgef_ids[mgef.get("key")] = form_id
        node = all_mgefs.get(form_id)
        check(node is not None and plugin_reader.editor_id(esp, node) == mgef.get("editorId"),
              f"missing cold magic effect {mgef.get('editorId')}")
        data = node.field(esp, b"DATA") if node is not None else None
        check(data is not None and len(data) >= 116, f"{mgef.get('editorId')} has no usable DATA")
        if data is not None and len(data) >= 116:
            check(struct.unpack_from("<I", data, 64)[0] == mgef.get("archetype", 34) and struct.unpack_from("<I", data, 0)[0] & 0x800
                  and struct.unpack_from("<I", data, 80)[0] == 0 and abs(struct.unpack_from("<f", data, 112)[0] - 1.0) < 1e-6,
                  f"{mgef.get('editorId')} must be an area-less constant actor-value modifier")
            check(struct.unpack_from("<I", data, 68)[0] == local_id(mgef.get("actorValueFormId", "0x0")),
                  f"{mgef.get('editorId')} targets the wrong actor value")
    for stage in cold_manifest.get("stages", []):
        node = all_spells.get(int(stage.get("spellFormId", "0x0"), 16))
        check(node is not None and plugin_reader.editor_id(esp, node) == stage.get("spellEditorId"),
              f"missing cold ability {stage.get('spellEditorId')}")
        if node is None:
            continue
        check(local_id(stage.get("spellFormId", "0x0")) == (cold_manifest.get("spellBase", 0) + stage.get("stage", 0) - 1) | 0x01000000,
              f"{stage.get('spellEditorId')} must sit at the ColdSpellBase offset for its stage")
        spit = node.field(esp, b"SPIT")
        check(spit is not None and len(spit) == 36 and struct.unpack_from("<I", spit, 8)[0] == 4
              and struct.unpack_from("<I", spit, 16)[0] == 0,
              f"{stage.get('spellEditorId')} must be a constant-effect ability")
        cold_effects = spell_effects(esp, node)
        wanted = {cold_mgef_ids.get(key): float(value) for key, value in stage.get("effects", {}).items()}
        found = {effect[0]: (effect[1][0] if effect[1] else None) for effect in cold_effects}
        check(len(cold_effects) == len(wanted) and found.keys() == wanted.keys()
              and all(found[key] is not None and abs(found[key] - wanted[key]) < 1e-5 for key in wanted),
              f"{stage.get('spellEditorId')} effects {found} diverged from the manifest {wanted}")
        check(all(effect[1] is not None and effect[1][1] == 0 and effect[1][2] == 0 for effect in cold_effects),
              f"{stage.get('spellEditorId')} must be permanent and area-less")

    hot_node = all_lists.get(int(hot_food.get("recordFormId", "0x0"), 16))
    check(hot_node is not None and plugin_reader.editor_id(esp, hot_node) == hot_food.get("editorId"),
          "missing the hot-food form list")
    if hot_node is not None:
        hot_actual = [int.from_bytes(esp[start:start + 4], "little")
                      for sig, start, size in hot_node.subrecords(esp) if sig == b"LNAM"]
        hot_expected = [local_id(entry.get("recordFormId", "0x0")) for entry in hot_food.get("entries", [])]
        check(len(hot_expected) == 4 and hot_actual == hot_expected, "the hot-food list entries diverged from the manifest")
        check(all(entry.get("recordFormId", "").endswith(":Fallout4.esm") for entry in hot_food.get("entries", [])),
              "hot-food entries must be Fallout4.esm forms, the plugin's only master")

    quest_vmad = scripts.get("RadSurvivalQuest", {})
    new_tuning = [(disease_script, cause.get("chanceProperty"), cause.get("chance")) for cause in causes.get("entries", [])]
    new_tuning += [(disease_script, causes.get("woundHealthFractionProperty"), causes.get("woundHealthFraction")),
                   (disease_script, causes.get("foodPoisoningClearProperty"), causes.get("foodPoisoningClearHours")),
                   (disease_script, "MaxDiseaseId", max_id), (disease_script, bases.get("spellProperty"), bases.get("spellBase")),
                   (disease_script, bases.get("perkProperty"), bases.get("perkBase")),
                   (quest_vmad, cold_manifest.get("spellBaseProperty"), cold_manifest.get("spellBase")),
                   (quest_vmad, cold_manifest.get("perkBaseProperty"), cold_manifest.get("perkBase")),
                   (quest_vmad, hot_food.get("property"), hot_food.get("formIdDecimal")),
                   (quest_vmad, hot_food.get("warmHoursProperty"), hot_food.get("warmHours"))]
    new_tuning += [(disease_script, item.get("property"), local_id(item.get("recordFormId", "0x0")))
                   for item in treatments.get("stepDown", []) + treatments.get("cureOutright", [])]
    for props, prop, expected in new_tuning:
        check(props.get(prop) is None or abs(float(props[prop]) - float(expected)) < 1e-6,
              f"{prop} is authored in the VMAD as {props.get(prop)} but the manifest says {expected}")

if failures:
    print("survival record contract failures:")
    for failure in failures:
        print("  FAIL", failure)
    sys.exit(1)

print("Survival records verified: taint lists and cure, seven diseases with perks, causes and treatments, cold ladder and hot food")
