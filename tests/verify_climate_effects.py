import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SCRIPTS = ROOT / "deploy" / "Scripts" / "Source" / "User"


def text(path):
    return path.read_text(encoding="utf-8")


def body(source, signature):
    start = source.index(signature)
    open_brace = source.index("{", start)
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace:index + 1]
    raise AssertionError(f"unterminated body for {signature}")


def papyrus_body(source, signature):
    start = source.index(signature)
    end = source.index("EndFunction", start)
    return source[start:end]


effects = text(SRC / "ClimateEffects.cpp")
effects_header = text(SRC / "ClimateEffects.h")
runtime = text(SRC / "ClimateRuntime.cpp")
runtime_header = text(SRC / "ClimateRuntime.h")
debug = text(SRC / "RadEngine_Debug.cpp")
engine_header = text(SRC / "RadiationEngine.h")
main = text(SRC / "main.cpp")
quest = text(SCRIPTS / "RadSurvivalQuest.psc")
showcase = text(SCRIPTS / "RadShowcase.psc")
natives = text(SCRIPTS / "RadNatives.psc")
policy = text(SRC / "ClimatePolicy.h")

for token in (
    "void InstallPapyrusBridge();",
    "void OnTick(RadClimatePolicy::HypothermiaStage stage, bool liveTick);",
    "[[nodiscard]] float WarmthFloor01(float gameDays) noexcept;",
    "void Resend() noexcept;",
    "void Reset() noexcept;",
):
    assert token in effects_header, f"climate effects interface missing: {token}"

dispatch = body(effects, "bool DispatchStage(int stage)")
assert 'vm->DispatchStaticCall("RadShowcase", "ApplyCold", noCallback, static_cast<std::int32_t>(stage))' in dispatch, (
    "stage changes must reach RadShowcase.ApplyCold")

sync = body(effects, "void SyncStage(int wanted)")
assert "wanted == g_sentStage && since < kResendInterval" in sync, "an unchanged stage must only be re-sent on the interval"
assert "g_sentStage == -2 && since < kRetryInterval" in sync, "a failed dispatch must be retried on a throttle"
assert "g_sentStage = sent ? wanted : -2;" in sync, "a failed dispatch must not be recorded as sent"

damage = body(effects, "void ApplyColdDamage(RadClimatePolicy::HypothermiaStage stage, float seconds)")
for token in (
    "RadClimatePolicy::EffectsForStage(stage).healthDamagePerSecond",
    "RadNeedsTuning::Value(RadNeedsTuningPolicy::Key::kHypothermiaDamage, 1.0f)",
    "RadiationEngine::GetSingleton().PlayerHealthDrain(player, av->health, -amount)",
    "player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *av->health, drain)",
    "player->IsDead(false)",
    "player->GetDifficultyLevel() == RE::DifficultyLevel::kTrueSurvival",
):
    assert token in damage, f"hypothermia damage contract missing: {token}"

tick = body(effects, "void ClimateEffects::OnTick(RadClimatePolicy::HypothermiaStage stage, bool liveTick)")
assert "ConsumePendingWarmth();" in tick, "the main-thread tick must consume warmth queued by Papyrus"
assert "SyncStage(on ? static_cast<int>(stage) : 0);" in tick, "the effects toggle must clear the Papyrus stage"
assert "if (on && liveTick)" in tick, "damage must only apply on live ticks with effects on"
assert "std::clamp(seconds, 0.0f, kMaxDamageStepSeconds)" in tick, "one tick must never apply more than a second of damage"
assert "kHypothermiaEffects" in effects, "the effects toggle must be read from tuning"

warm = body(effects, "void PapyrusWarmBody(std::monostate, float a_hours)")
assert "std::isfinite(a_hours)" in warm and "a_hours <= 0.0f" in warm, "WarmBody must reject bad input"
assert "g_pendingWarmHours.compare_exchange_weak" in warm, "the Papyrus thread must only queue warmth"
assert "ModActorValue" not in warm and "GameDays()" not in warm, "the Papyrus thread must not touch game state"
assert '"RadNatives", "WarmBody", PapyrusWarmBody' in effects
floor = body(effects, "float ClimateEffects::WarmthFloor01(float gameDays) noexcept")
assert "kHotFoodHeat01" in floor and "gameDays < until" in floor, "the hot-food floor must expire at its end time"
assert re.search(r"kHotFoodHeat01 = 0\.6f", effects), "the hot-food heat floor is 0.6"
reset = body(effects, "void ClimateEffects::Reset() noexcept")
assert "Resend();" in reset and "g_warmUntilDays.store(-1.0f" in reset and "g_pendingWarmHours.store(0.0f" in reset, (
    "reset must clear warmth and force a stage resend")

update = body(runtime, "void ClimateRuntime::Update()")
assert "environment.heat01 = std::max(environment.heat01, ClimateEffects::WarmthFloor01(GameDays()));" in update
assert update.index("environment.heat01 = _heat.Sample(player);") < update.index("ClimateEffects::WarmthFloor01")
assert 'ClimateEffects::OnTick(_state.stage, std::strcmp(branchName, "normal") == 0);' in update, (
    "only the normal branch is a live tick; pauses and sleep/wait jumps must not damage")
assert update.index("nextState.stage = RadClimateStagePolicy::NextStage") < update.index("ClimateEffects::OnTick(")
assert "ClimateEffects::Reset();" in body(runtime, "void ClimateRuntime::Reset()")
assert "ClimateEffects::Resend();" in body(runtime, "void ClimateRuntime::RestoreFromSave(")
force = body(runtime, "void ClimateRuntime::DebugForceStage(int stage) noexcept")
assert "{ 37.0f, 36.0f, 35.0f, 33.75f, 31.5f, 29.0f }" in force, "debug stages use the middle of each core band"
assert "std::clamp(stage, 0, 5)" in force and "ClimateEffects::Resend();" in force
assert "void DebugForceStage(int stage) noexcept;" in runtime_header

stage_for_core = body(policy, "inline HypothermiaStage StageForCore(float coreC) noexcept")
bands = [float(v) for v in re.findall(r"coreC >= ([0-9.]+)f", stage_for_core)]
assert bands == [36.5, 35.5, 34.5, 33.0, 30.0], f"stage thresholds moved: {bands}"
mids = [37.0, 36.0, 35.0, 33.75, 31.5, 29.0]
edges = [99.0] + bands + [-99.0]
for stage, mid in enumerate(mids):
    assert edges[stage + 1] <= mid < edges[stage], f"stage {stage} mid {mid} is outside its band"

assert 'a.rfind("climate:stage:", 0) == 0' in debug and "DebugForceStage(stage)" in debug
assert 'a == "climate:warm"' in debug and "DebugForceStage(0)" in debug
assert "float PlayerHealthDrain(RE::PlayerCharacter* player, RE::ActorValueInfo* healthAv, float kDamage) const" in engine_header
public_part = engine_header.split("private:", 1)[0]
assert "PlayerHealthDrain(" in public_part, "PlayerHealthDrain must be public"
god_check = body(text(SRC / "RadEngine_Storm_FX.cpp"), "static bool IsPlayerGodMode(RE::PlayerCharacter* player)")
assert "IsRuntime" not in god_check and "RE::ID::PlayerCharacter::IsGodMode" in god_check, "TGM detection must run on OG and AE"
assert "_debugGod || (_respectGodMode && IsPlayerGodMode(player))" in text(SRC / "RadEngine_Storm_FX.cpp"), "PlayerGodMode must cover the debug switch and TGM"
assert "ClimateEffects::InstallPapyrusBridge();" in main

assert "Function WarmBody(float afHours) global native" in natives
apply_cold = papyrus_body(showcase, "Function ApplyCold(int aiStage)")
assert "ApplyColdStage(aiStage)" in apply_cold
cold = papyrus_body(quest, "Function ApplyColdStage(int aiStage)")
for token in ("aiStage < 0 || aiStage > 5 || !IsActive()", "Diseases.SetColdStage(aiStage)",
              "ColdSpellBase + aiStage - 1", "ColdPerkBase + aiStage - 1",
              "player.RemoveSpell(AppliedColdSpell)", "player.AddSpell(wanted, false)",
              "player.RemovePerk(AppliedColdPerk)"):
    assert token in cold, f"ApplyColdStage contract missing: {token}"
warm_food = papyrus_body(quest, "Function WarmFromFood(")
assert "HotFoodListFormId" in warm_food and "RadNatives.WarmBody(" in warm_food
assert "WarmFromFood(" in papyrus_body(quest, "Function ApplyRelief(Form akItem, int aiCount)")

print("climate effects verified: Papyrus stage dispatch, god-mode-safe cold damage, hot-food warmth floor, debug stages")
