#include "RadEngine_Internal.h"
#include "Hooks.h"
#include "SerializationPolicy.h"

using namespace RadInternal;

void RadiationEngine::ApplyDebuffs(uint32_t radLevel)
{
    auto logger = Log();
    auto player = RE::PlayerCharacter::GetSingleton();

    if (!player)
    {
        logger->debug("[ApplyDebuffs] Player not available");
        return;
    }

    const auto* stage = _config->GetStageConfig(radLevel);
    if (!stage)
    {
        if (_currentStageLevel != 0)
            logger->warn("[ApplyDebuffs] No stage config for rad level {} — clearing stage effects.", radLevel);
        _currentStageLevel = 0;
        UpdateStagePerk(-1);
        ApplyStatModification(-1);
        return;
    }

    uint32_t stageThreshold = (radLevel / 100) * 100;
    if (stageThreshold > 1000) stageThreshold = 1000;

    const int stageIndex = (stageThreshold >= 100) ? static_cast<int>(stageThreshold / 100) - 1 : -1;

    if (stageThreshold != _currentStageLevel)
    {
        const int endPen = (stageIndex >= 0) ? kStagePenalties[stageIndex].end : 0;
        const int strPen = (stageIndex >= 0) ? kStagePenalties[stageIndex].str : 0;
        _currentStageLevel = stageThreshold;
        logger->warn("========== RADIATION STAGE CHANGED ==========");
        logger->warn("  Radiation Level: {} rad", radLevel);
        logger->warn("  Stage: {}", stage->name);
        logger->warn("  Applied Penalties: END -{} / STR -{}", endPen, strPen);
        logger->warn("===========================================");
    }

    UpdateStagePerk(stageIndex);

    ApplyStatModification(stageIndex);
}


void RadiationEngine::UpdateStagePerk(int stageIndex)
{
    EnsureEslLoaded();
    if (!_eslAvailable) return;
    if (stageIndex == _appliedPerkIndex) return;

    auto logger = Log();
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player)
    {
        logger->warn("[ESL] UpdateStagePerk: player singleton NULL; deferring perk swap.");
        return;
    }

    logger->info("[ESL] Stage perk transition: index {} -> {}", _appliedPerkIndex, stageIndex);

    for (int i = 0; i < kStageCount; ++i)
    {
        if (i == stageIndex) continue;
        RE::BGSPerk* p = _stagePerks[i];
        if (p && player->GetPerkRank(p) > 0)
        {
            player->RemovePerk(p);
            logger->debug("[ESL]   REMOVE \"{}\" (0x{:08X}) -> rank now {}",
                PerkName(p), p->GetFormID(), player->GetPerkRank(p));
        }
    }

    if (stageIndex >= 0 && stageIndex < kStageCount && _stagePerks[stageIndex])
    {
        RE::BGSPerk* now = _stagePerks[stageIndex];
        if (player->GetPerkRank(now) == 0)
        {
            player->AddPerk(now, 1);
        }
        const uint8_t rankAfter = player->GetPerkRank(now);
        logger->info("[ESL]   ADD \"{}\" (0x{:08X}) rank now {} {}",
            PerkName(now), now->GetFormID(), rankAfter,
            rankAfter > 0 ? "[ok — visible in PERKS + STATUS]" : "[WARN AddPerk did not stick]");
    }
    else if (stageIndex < 0)
    {
        logger->info("[ESL]   No active radiation stage — all stage perks cleared (cured).");
    }

    _appliedPerkIndex = stageIndex;
}


void RadiationEngine::ApplyRestTissueHeal(float gameHoursElapsed, float currentRads)
{
    if (_prevGameDays < 0.0f) return;
    if (!_tissueGlobal || gameHoursElapsed <= 0.0f) return;
    if (currentRads >= kTissueHealBelow) return;
    if (_tissueDamage <= _permanentTissueDamage) return;

    const float heal = gameHoursElapsed * kRestTissueHealPerGameHour;
    if (heal > 0.0f)
    {
        Log()->info("[Rest] {:.1f} game-hours elapsed -> healing {:.1f} tissue.",
            gameHoursElapsed, heal);
        HealTissue(heal, "rest");
    }
}


void RadiationEngine::HealTissue(float amount, const char* source)
{
    if (amount <= 0.0f || !_tissueGlobal) return;
    float before = _tissueDamage;
    _tissueDamage = std::max(_permanentTissueDamage, _tissueDamage - amount);
    float actual = before - _tissueDamage;
    if (actual <= 0.0f) return;
    _tissueGlobal->value = _tissueDamage;
    Log()->info("[TissueCure:{}] -{:.1f} ({:.1f} -> {:.1f}, perm floor {:.1f})",
        source, actual, before, _tissueDamage, _permanentTissueDamage);
}


void RadiationEngine::HealPermanentTissue(float amount, const char* source)
{
    if (amount <= 0.0f || !_permanentTissueGlobal) return;
    float before = _permanentTissueDamage;
    _permanentTissueDamage = std::max(0.0f, _permanentTissueDamage - amount);
    _permanentTissueGlobal->value = _permanentTissueDamage;
    if (_tissueGlobal && _tissueDamage > _permanentTissueDamage)
    {
        _tissueDamage -= (before - _permanentTissueDamage);
        if (_tissueDamage < _permanentTissueDamage) _tissueDamage = _permanentTissueDamage;
        _tissueGlobal->value = _tissueDamage;
    }
    Log()->info("[PermCure:{}] -{:.1f} perm ({:.1f} -> {:.1f})",
        source, before - _permanentTissueDamage, before, _permanentTissueDamage);
}


void RadiationEngine::CheckActiveCures(float currentRads, float currentHP, bool deltaValid)
{
    {
        static bool  s_filterJumpInit = false;
        static float s_prevFilterGameDays = 0.0f;
        static bool  s_prevFilterMasked = false;
        static bool  s_prevFilterExposed = false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        const float nowDays = GameDays();
        const bool maskedNow = player && IsWearingGasMask();
        const bool exposedNow = player && !IsPlayerSheltered() && !IsPlayerInPoweredShelter() &&
                                !IsPlayerInPowerArmor();

        const bool reseed = !s_filterJumpInit || _prevRads < 0.0f || _prevHP < 0.0f || nowDays <= 0.0f;
        if (!reseed && !deltaValid && nowDays > s_prevFilterGameDays &&
            _filterItem && _filterEnabled && s_prevFilterMasked && s_prevFilterExposed)
        {
            const float gameHours = (nowDays - s_prevFilterGameDays) * 24.0f;
            float timeScale = 0.0f;
            if (auto* cal = RE::Calendar::GetSingleton(); cal && cal->timeScale)
                timeScale = cal->timeScale->GetValue();

            if (gameHours > 0.0f && timeScale > 0.0f && _filterDurationSec > 0)
            {
                const float realSeconds = gameHours * 3600.0f / timeScale;
                const float drainPct = realSeconds * (100.0f / static_cast<float>(_filterDurationSec));
                if (drainPct > 0.0f)
                {
                    const float before = _filterCharge;
                    int replacementsUsed = 0;

                    if (drainPct < _filterCharge)
                    {
                        _filterCharge -= drainPct;
                    }
                    else
                    {
                        const float remainingAfterCurrent = drainPct - _filterCharge;
                        _filterCharge = 0.0f;

                        if (_filterAutoReplace && player)
                        {
                            const int spares = FilterItemCount();
                            const int needed = static_cast<int>(std::floor(remainingAfterCurrent / 100.0f)) + 1;
                            replacementsUsed = std::min(spares, std::max(0, needed));
                            if (replacementsUsed > 0)
                            {
                                RE::TESObjectREFR::RemoveItemData data(_filterItem, replacementsUsed);
                                player->RemoveItem(data);
                                _filterCharge = std::clamp(
                                    static_cast<float>(replacementsUsed) * 100.0f - remainingAfterCurrent,
                                    0.0f, 100.0f);
                            }
                        }
                    }

                    _filterLowWarned = 0;
                    Log()->info(
                        "[Filter/TimeJump] {:.1f} game-hours @ timescale {:.1f} = {:.1f}s real; "
                        "charge {:.1f}% -> {:.1f}% (auto replacements {}).",
                        gameHours, timeScale, realSeconds, before, _filterCharge, replacementsUsed);
                }
            }
            else if (gameHours > 0.0f && timeScale <= 0.0f)
            {
                Log()->warn("[Filter/TimeJump] timescale unavailable/non-positive; skipped filter drain.");
            }
        }

        s_filterJumpInit = true;
        s_prevFilterGameDays = nowDays;
        s_prevFilterMasked = maskedNow;
        s_prevFilterExposed = exposedNow;
    }

    if (!deltaValid)
    {
        _radAwayWindowTicks = 0;
        return;
    }
    if (_prevRads < 0.0f || _prevHP < 0.0f) return;

    const float radDrop = _prevRads - currentRads;
    const float hpGain  = currentHP - _prevHP;

    if (_radAwayWindowTicks > 0)
    {
        if (radDrop > 15.0f && hpGain < 20.0f)
            HealTissue(radDrop * 0.12f, "radaway");
        --_radAwayWindowTicks;
    }
}


void RadiationEngine::NotifyVerifiedCureUse(RadCorrectnessPolicy::CureSignal signal)
{
    if (!Hooks::IsGameMainThread())
    {
        Log()->error("[TissueCure] Ignoring verified cure signal off the game main thread.");
        return;
    }

    if (RadCorrectnessPolicy::CuresPermanentTissue(signal))
    {
        HealPermanentTissue(_permanentTissueDamage, "chelation");
        HealTissue(_tissueDamage, "chelation");
        return;
    }

    if (RadCorrectnessPolicy::ArmsRadAwayHealing(signal))
    {
        _radAwayWindowTicks = kRadAwayWindowTicks;
        Log()->debug("[RadAway] verified use; tissue-heal window armed for ~20 s.");
    }
}


float RadiationEngine::UpdateTissueDamage(float particulateRads)
{
    if (!_tissueGlobal)
        return particulateRads;

    if (particulateRads > kTissueHealBelow)
    {
        const float sev = std::clamp(_tissueDamage / kTissueMax, 0.0f, 1.0f);
        _tissueDamage += particulateRads * kTissueGainRate * (1.0f + sev * kTissueSpiral);
    }
    else
    {
        _tissueDamage -= kTissueHealRate;
    }

    if (_tissueDamage < _permanentTissueDamage) _tissueDamage = _permanentTissueDamage;
    if (_tissueDamage > kTissueMax)             _tissueDamage = kTissueMax;

    if (_permanentTissueGlobal && _tissueDamage > kPermAccrualThresh)
    {
        _permanentTissueDamage += kPermAccrualRate * (_tissueDamage / kPermAccrualThresh);
        if (_permanentTissueDamage > _tissueDamage) _permanentTissueDamage = _tissueDamage;
        _permanentTissueGlobal->value = _permanentTissueDamage;
    }

    _tissueGlobal->value = _tissueDamage;

    return particulateRads + _tissueDamage * kTissueWeight;
}


void RadiationEngine::ApplyStatModification(int stageIndex)
{
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto* actorValue = RE::ActorValue::GetSingleton();
    if (!actorValue) return;

    int wantEnd = (stageIndex >= 0 && stageIndex < kStageCount) ? kStagePenalties[stageIndex].end : 0;
    int wantStr = (stageIndex >= 0 && stageIndex < kStageCount) ? kStagePenalties[stageIndex].str : 0;

    int scarEnd = 0;
    if (_permanentTissueDamage > 0.0f)
    {
        const float scar01 = std::clamp(_permanentTissueDamage / kTissueMax, 0.0f, 1.0f);
        scarEnd = static_cast<int>(std::lround(kScarEndMax * scar01));
    }

    if (_specialSuspendStage >= 0)
    {
        if (stageIndex == _specialSuspendStage)
        {
            wantEnd = 0;
            wantStr = 0;
        }
        else
        {
            Log()->info(
                "[FixStats] stage changed {} -> {}; resuming SPECIAL penalty.",
                _specialSuspendStage, stageIndex);
            _specialSuspendStage = -1;
        }
    }

    wantEnd += scarEnd;

    ApplyClampedSpecial(player, actorValue->endurance, wantEnd, _appliedEndPenalty);
    ApplyClampedSpecial(player, actorValue->strength,  wantStr, _appliedStrPenalty);
}


void RadiationEngine::ApplyTrackedAV(RE::PlayerCharacter* player, RE::ActorValueInfo* av,
                                     float wantPenalty, float& applied)
{
    if (!player || !av) return;
    const float perm    = player->GetPermanentActorValue(*av);
    const float desired = std::clamp(wantPenalty, 0.0f, std::max(0.0f, perm));
    if (std::fabs(desired - applied) < 0.5f) return;
    player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *av, applied - desired);
    applied = desired;
}


void RadiationEngine::WantedTissuePenalties(float (&a_out)[4]) const
{
    const float t   = std::clamp(_tissueDamage / kTissueMax, 0.0f, 1.0f);
    const float spd = (t > kTissueSpeedFrom) ? (t - kTissueSpeedFrom) / (1.0f - kTissueSpeedFrom) : 0.0f;
    const float fat = (t > kTissueApFrom)    ? (t - kTissueApFrom)    / (1.0f - kTissueApFrom)    : 0.0f;
    a_out[0] = kTissueCarryMax * t;
    a_out[1] = kTissueHealMax  * t;
    a_out[2] = kTissueSpeedMax * spd;
    a_out[3] = kTissueApMax    * fat;
}


void RadiationEngine::ApplyTissueEffects()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av     = RE::ActorValue::GetSingleton();
    if (!player || !av || !_tissueGlobal) return;

    const float t = std::clamp(_tissueDamage / kTissueMax, 0.0f, 1.0f);

    float want[4];
    WantedTissuePenalties(want);
    ApplyTrackedAV(player, av->carryWeight,  want[0], _appliedCarryPen);
    ApplyTrackedAV(player, av->healRateMult, want[1], _appliedHealPen);
    ApplyTrackedAV(player, av->speedMult,    want[2], _appliedSpeedPen);
    ApplyTrackedAV(player, av->actionPoints, want[3], _appliedApPen);

    UpdateTissueSymptoms(t);
}


void RadiationEngine::UpdateTissueSymptoms(float t)
{
    if (t < kSymCoughFrom)
    {
        _symptomWarned = 0;
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av     = RE::ActorValue::GetSingleton();
    if (!player) return;

    if (t >= kSymVomitFrom && _symptomWarned < 2)
    {
        Notify("You double over retching - the sickness is deep in you now.", true);
        _symptomWarned = 2;
    }
    else if (t >= kSymNauseaFrom && t < kSymVomitFrom && _symptomWarned < 1)
    {
        Notify("A wave of nausea rolls through you.", true);
        _symptomWarned = 1;
    }

    if (--_symptomTick > 0) return;
    static std::mt19937 rng{ std::random_device{}() };
    const int span = kSymMaxTicks - kSymMinTicks;
    _symptomTick = kSymMinTicks +
        std::uniform_int_distribution<int>(0, std::max(1, static_cast<int>(span * (1.0f - t))))(rng);

    const bool female = IsPlayerFemale();
    RE::BSSoundHandle h{};
    Play2DSound(female ? kSndChokeNoMaskF : kSndChokeNoMaskM, h);

    if (t >= kSymVomitFrom && av && av->health &&
        player->GetActorValue(*av->health) - kSymVomitHp >= kSymVomitFloorHp)
        player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *av->health, -kSymVomitHp);
}


void RadiationEngine::ApplyClampedSpecial(RE::PlayerCharacter* player, RE::ActorValueInfo* av,
                                          int wantPenalty, std::int32_t& applied)
{
    if (!player || !av) return;

    const float perm       = player->GetPermanentActorValue(*av);
    const int   maxPenalty = std::max(0, static_cast<int>(perm) - 1);
    const int   desired    = std::min(wantPenalty, maxPenalty);

    if (desired != applied)
    {
        const float delta = static_cast<float>(applied - desired);
        player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *av, delta);
        Log()->debug("[SPECIAL] penalty {} -> {} (perm {:.0f}, delta {:+.0f})",
            applied, desired, perm, delta);
        applied = desired;
    }
}


void RadiationEngine::RemoveStatModification()
{
    auto player = RE::PlayerCharacter::GetSingleton();
    auto* actorValue = RE::ActorValue::GetSingleton();
    if (!player || !actorValue) return;

    if (actorValue->endurance && _appliedEndPenalty != 0)
    {
        player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *actorValue->endurance, static_cast<float>(_appliedEndPenalty));
        _appliedEndPenalty = 0;
    }
    if (actorValue->strength && _appliedStrPenalty != 0)
    {
        player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *actorValue->strength, static_cast<float>(_appliedStrPenalty));
        _appliedStrPenalty = 0;
    }
}


void RadiationEngine::RepairLeakedCarryWeight()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av     = RE::ActorValue::GetSingleton();
    if (!player || !av || !av->carryWeight) return;

    const float total     = player->GetActorValue(*av->carryWeight);
    const float permanent = player->GetPermanentActorValue(*av->carryWeight);

    if (!RadiationSerialization::RestoredCarryPenaltyHeld(_appliedCarryPen, total, permanent))
    {
        Log()->warn("[Load] Restored carry penalty {:.1f} but the game holds none (total {:.1f}, base {:.1f}) - dropping restored penalties.",
            _appliedCarryPen, total, permanent);
        _appliedCarryPen = _appliedHealPen = _appliedSpeedPen = _appliedApPen = 0.0f;
    }

    if (!_penaltiesFromSave)
    {
        float want[4];
        WantedTissuePenalties(want);
        want[0] = std::min(want[0], permanent);
        if (want[0] >= 0.5f && std::isfinite(total) && permanent - total >= want[0])
        {
            _appliedCarryPen = want[0];
            _appliedHealPen  = want[1];
            _appliedSpeedPen = want[2];
            _appliedApPen    = want[3];
            Log()->info("[Load] No penalty record in save; inferred penalties from tissue (carry {:.1f}, heal {:.1f}, speed {:.1f}, AP {:.1f}).",
                want[0], want[1], want[2], want[3]);
        }
    }

    const float delta = RadiationSerialization::CarryLeakRepairDelta(total, permanent);
    if (delta <= 0.0f) return;

    _appliedCarryPen = 0.0f;
    player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *av->carryWeight, delta);
    Log()->warn("[RepairCarry] carry weight was negative; raised by {:.0f} to the permanent base.", delta);
    Notify("Restored leaked negative carry weight.", false);
}


void RadiationEngine::DispelNegativeSpecials()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av     = RE::ActorValue::GetSingleton();
    if (!player || !av) return;

    struct AV { RE::ActorValueInfo* info; const char* name; };
    const AV specials[] = {
        { av->strength, "STR" }, { av->perception, "PER" }, { av->endurance, "END" },
        { av->charisma, "CHA" }, { av->intelligence, "INT" }, { av->agility, "AGI" }, { av->luck, "LCK" },
    };
    auto logger = Log();
    int fixed = 0;
    for (const auto& s : specials)
    {
        if (!s.info) continue;
        const float total = player->GetActorValue(*s.info);
        if (total >= 1.0f) continue;
        const float perm = player->GetPermanentActorValue(*s.info);
        if (perm < 1.0f) continue;
        player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *s.info, perm - total);
        logger->warn("[DispelNeg] {} was {:.0f} (base {:.0f}) — restored to base.", s.name, total, perm);
        ++fixed;
    }
    if (fixed > 0)
    {
        _appliedEndPenalty = 0;
        _appliedStrPenalty = 0;
        char m[80];
        std::snprintf(m, sizeof(m), "Restored %d corrupted SPECIAL stat%s.", fixed, fixed == 1 ? "" : "s");
        Notify(m, false);
    }
}
