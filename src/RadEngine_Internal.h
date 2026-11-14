#pragma once

#include "pch.h"
#include "RadiationEngine.h"
#include "RadiationConfig.h"
#include "RadHud.h"

#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESGlobal.h>
#include <RE/A/ActorValue.h>
#include <RE/A/ActorValueOwner.h>
#include <RE/A/ACTOR_VALUE_MODIFIER.h>
#include <RE/A/ActorEquipManager.h>
#include <RE/B/BGSObjectInstance.h>
#include <RE/B/BGSReferenceEffect.h>
#include <RE/B/BGSArtObject.h>
#include <RE/T/TESImageSpace.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BGSKeyword.h>
#include <RE/B/BGSPerk.h>
#include <RE/B/BipedAnim.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/B/BGSBipedObjectForm.h>
#include <RE/B/BIPED_OBJECT.h>
#include <RE/B/BGSListForm.h>
#include <RE/B/BGSInventoryList.h>
#include <RE/B/BGSInventoryItem.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESHitEvent.h>
#include <RE/B/BSTEvent.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESImageSpaceModifier.h>
#include <RE/I/ImageSpaceModifierInstanceForm.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TESQuest.h>
#include <RE/G/GameScript.h>
#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/B/BSScriptUtil.h>
#include <RE/B/BSScript_IStackCallbackFunctor.h>
#include <RE/B/BSScript_IObjectHandlePolicy.h>
#include <RE/B/BSScript_Object.h>
#include <RE/B/BSScript_Variable.h>
#include <RE/B/BSScript_Internal_VirtualMachine.h>
#include <RE/B/BSScript_Internal_AttachedScript.h>
#include <RE/C/Calendar.h>
#include <RE/S/Sky.h>
#include <RE/T/TESWeather.h>
#include <RE/B/BSAudioManager.h>
#include <RE/B/BSSoundHandle.h>
#include <RE/B/BSResource_ID.h>
#include <RE/A/Actor.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWorldSpace.h>
#include <RE/T/TESWeather.h>
#include <RE/B/bhkPickData.h>
#include <RE/B/BSHavok.h>
#include <RE/C/COL_LAYER.h>
#include <RE/N/NiPoint3.h>
#include <RE/U/UI.h>
#include <RE/P/ProcessLists.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESQuest.h>
#include <RE/T/TESBoundObject.h>
#include <RE/I/ITEM_REMOVE_REASON.h>
#include <RE/B/BGSHazard.h>
#include <RE/B/BGSExplosion.h>
#include <RE/N/NEW_REFR_DATA.h>
#include <RE/M/MagicTarget.h>
#include <RE/A/ActiveEffect.h>
#include <RE/A/ActiveEffectList.h>
#include <RE/E/EffectItem.h>
#include <RE/E/EffectSetting.h>
#include <RE/M/MagicItem.h>

#include <random>
#include <cstdio>
#include <cstdlib>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace RadInternal
{
    inline spdlog::logger* Log()
    {
        static spdlog::logger* s = spdlog::get("FalloutAnomaly").get();
        return s;
    }

    constexpr const char* kEslName = "RadiationOverhaul.esp";
    constexpr uint32_t     kPerkBaseFormID  = 0x820;
    constexpr uint32_t     kRadWeatherListID     = 0x802;
    constexpr uint32_t     kGasMaskFilterID      = 0x803;
    constexpr uint32_t     kGasMaskRepairKitID   = 0x804;
    constexpr uint32_t     kGasMaskKeywordID     = 0x809;
    constexpr int          kNpcMaskRescanTicks   = 20;
    constexpr uint32_t     kTissueGlobalID       = 0x850;
    constexpr uint32_t     kPermTissueGlobalID   = 0x851;
    constexpr uint32_t     kStormFXQuestFormID   = 0x871;

    constexpr int kFixStatsReliefPerPress = 5;

    constexpr float kTissueGainRate    = 0.00009f;
    constexpr float kTissueHealRate    = 0.10f;
    constexpr float kTissueHealBelow   = 150.0f;
    constexpr float kTissueMax         = 1000.0f;
    constexpr float kTissueWeight      = 1.0f;
    constexpr float kPermAccrualThresh = 200.0f;
    constexpr float kPermAccrualRate   = 0.006f;

    constexpr float kTissueCarryMax  = 120.0f;
    constexpr float kTissueHealMax   = 50.0f;
    constexpr float kTissueSpeedMax  = 15.0f;
    constexpr float kTissueSpeedFrom = 0.50f;
    constexpr float kTissueApMax     = 40.0f;
    constexpr float kTissueApFrom    = 0.40f;
    constexpr float kTissueSpiral    = 1.50f;

    constexpr int   kScarEndMax      = 4;

    constexpr float kSymCoughFrom    = 0.30f;
    constexpr float kSymNauseaFrom   = 0.55f;
    constexpr float kSymVomitFrom    = 0.75f;
    constexpr float kSymVomitHp      = 6.0f;
    constexpr float kSymVomitFloorHp = 20.0f;
    constexpr int   kSymMinTicks     = 40;
    constexpr int   kSymMaxTicks     = 140;
    constexpr float kRestTissueHealPerGameHour = 40.0f;

    constexpr uint32_t kRadstormWeathers[] = { 0x1C3D5E, 0x2392A3, 0x222394, 0x24A3C0 };
    constexpr uint32_t kStormRadSkyWeatherPrimary  = 0x24A3C0;
    constexpr uint32_t kStormRadSkyWeatherFallback = 0x1C3D5E;

    inline bool IsRadstormWeather(uint32_t formID)
    {
        for (uint32_t rw : kRadstormWeathers)
            if (formID == rw) return true;
        return false;
    }

    inline bool IsRainyWeather(RE::TESWeather* w)
    {
        if (!w) return false;
        const auto flags = w->weatherData[static_cast<int>(RE::TESWeather::WeatherData::kFlags)];
        return (flags & static_cast<std::int8_t>(RE::TESWeather::WeatherDataFlags::kRainy)) != 0;
    }

    constexpr float kRadstormVetoMaxPct = 0.10f;

    constexpr const char* kStormSoundPath = "Sound\\FalloutAnomaly\\ambient\\psi_storm_siren.xwm";

    constexpr const char* kSndNukeImpact   = "Sound\\FalloutAnomaly\\ambient\\blowout\\blowout_impact.xwm";
    constexpr const char* kSndBlastWave    = "Sound\\FalloutAnomaly\\ambient\\blowout\\blowout_wave_01.xwm";

    inline constexpr const char* kBedAshFallout  = "Sound\\FalloutAnomaly\\ambient\\blowout\\blowout_wind_01.xwm";
    inline constexpr const char* kBedFirestorm   = "Sound\\FalloutAnomaly\\anomaly\\psi_storm_01.xwm";
    inline constexpr const char* kBedSulfurStorm = "Sound\\FalloutAnomaly\\ambient\\blowout\\blowout_bkgrnd.xwm";
    inline constexpr const char* kBedAcidRain    = "Sound\\FalloutAnomaly\\ambient\\blowout\\blowout_wind_02.xwm";
    inline constexpr const char* kBedDustStorm   = "Sound\\FalloutAnomaly\\ambient\\blowout\\blowout_wind_03.xwm";
    inline constexpr const char* kAmbientBedPath = "Sound\\FalloutAnomaly\\ambient\\blowout\\envcreepywind.xwm";

    constexpr const char* kSndMaskDrawM      = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_on_m.xwm";
    constexpr const char* kSndMaskDrawF      = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_on_f.xwm";
    constexpr const char* kSndMaskOffM       = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_off_m.xwm";
    constexpr const char* kSndMaskOffF       = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_off_f.xwm";
    constexpr const char* kSndFilterA        = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_breath_slow_a.xwm";
    constexpr const char* kSndFilterB        = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_breath_slow_b.xwm";
    constexpr const char* kSndFilterFastA    = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_breath_fast_a.xwm";
    constexpr const char* kSndFilterFastB    = "Sound\\FalloutAnomaly\\mask\\ashfall_mask_breath_fast_b.xwm";
    constexpr const char* kSndChokeNoMaskM   = "Sound\\FalloutAnomaly\\mask\\ashfall_choke_male_open.xwm";
    constexpr const char* kSndChokeNoMaskF   = "Sound\\FalloutAnomaly\\mask\\ashfall_choke_female_open.xwm";
    constexpr const char* kSndChokeWithMaskM = "Sound\\FalloutAnomaly\\mask\\ashfall_choke_male_masked.xwm";
    constexpr const char* kSndChokeWithMaskF = "Sound\\FalloutAnomaly\\mask\\ashfall_choke_female_masked.xwm";

    inline bool IsPlayerFemale()
    {
        auto* pc  = RE::PlayerCharacter::GetSingleton();
        auto* npc = pc ? pc->GetNPC() : nullptr;
        return npc && npc->IsFemale();
    }

    inline bool Play2DSound(const char* path, RE::BSSoundHandle& out)
    {
        auto* audio = RE::BSAudioManager::GetSingleton();
        if (!audio) { spdlog::warn("[Sound] no BSAudioManager for '{}'", path); return false; }
        RE::BSResource::ID file(path);
        audio->GetSoundHandleByFile(out, file, 0x10, 0);
        if (out.IsValid()) { out.Play(); return true; }
        spdlog::warn("[Sound] GetSoundHandleByFile failed (no handle) for '{}'", path);
        return false;
    }

    constexpr uint32_t kStormImadBase        = 0x880;
    constexpr uint32_t kNextEmissionGlobalID = 0x852;
    constexpr uint32_t kForceStormGlobalID   = 0x853;
    constexpr int      kExposedNagTicks      = 5 * 4;
    constexpr std::uint16_t kStormSoundFadeMs = 4000;

    constexpr std::uint16_t kRadstormCueFadeMs      = 2500;
    constexpr std::uint16_t kRadstormCueFalloffMs   = 2000;
    constexpr std::uint16_t kRadstormCueAttenuation = 1200;

    constexpr int      kNukeRollInTicks      = 12 * 4;
    constexpr int      kFirestormNukeMaxAttempts = 40;
    constexpr int      kFirestormNukeMaxWaitTicks = 40;
    constexpr int      kDebugMenuKey         = 0x77;

    constexpr float kStormMinRealMin = 35.0f;
    constexpr float kStormMaxRealMin = 90.0f;

    constexpr double kRadstormMinGapSec = 60.0 * 60.0;

    constexpr int kCellExitGraceTicks = 60 * 4;

    constexpr double kRadstormStartupGraceSec = 30.0 * 60.0;

    constexpr float kCureGapDays = 0.01f;

    constexpr float kStormGhoulHeal     = 150.0f;

    constexpr uint32_t kActorTypeGhoulKeyword = 0x00013794;

    constexpr int   kStormGhoulMinCount    = 5;
    constexpr int   kStormGhoulMaxCount    = 20;
    constexpr float kStormGhoulMinRadius   = 800.0f;
    constexpr float kStormGhoulMaxRadius   = 1500.0f;
    constexpr float kGhoulDespawnDist      = 10000.0f;
    constexpr int   kGhoulCleanupTicks     = 8;

    constexpr float kFleeRadius   = 5000.0f;
    constexpr float kFleeFromDist = 4096.0f;
    constexpr float kFleeToDist   = 8192.0f;
    constexpr int   kFleeRecheckTicks = 20;

    constexpr float kStormResistHalf = 80.0f;

    constexpr float kPostwarMinYear = 2100.0f;

    struct StormType
    {
        uint32_t    weatherID;
        const char* name;
        int         themeId;
        bool        lethal;
        float       radsPerTick;
        float       healthPerTick;
        int         warningTicks;
        int         activeTicks;
        int         aftermathTicks;
        int         weight;
        float       fogNear;
        float       fogFar;
        float       fogPower;
        float       fogHeightMid;
        float       fogHeightRange;
        float       fogHiDensity;
        const char* ambient;
    };

    constexpr StormType kStormTypes[] = {
        { 0x840, "Ash Fallout",        4,     true,   18.0f, 2.5f, 20 * 4,  120 * 4,  30 * 4, 15,  1200.0f, 8000.0f, 0.45f, 128.0f,  2000.0f, 0.30f, kBedAshFallout  },
        { 0x841, "Rad Firestorm",      1,     true,   14.0f, 4.0f, 20 * 4,  100 * 4,  30 * 4, 15,  1200.0f, 4500.0f, 0.60f, 128.0f,  2000.0f, 0.25f, kBedFirestorm   },
        { 0x842, "Toxic Sulfur Storm", 2,     false,  4.0f,  0.5f, 15 * 4,  180 * 4,  20 * 4, 35,  1000.0f, 3200.0f, 0.55f, 128.0f,  2000.0f, 0.20f, kBedSulfurStorm },
        { 0x843, "Acid Rain",          3,     false,  3.0f,  0.8f, 15 * 4,  180 * 4,  20 * 4, 35,  1400.0f, 6000.0f, 0.65f, 128.0f,  2000.0f, 0.45f, kBedAcidRain    },
        { 0x844, "Dust Storm",         5,     false,  3.0f,  0.3f, 15 * 4,  150 * 4,  25 * 4, 30,   150.0f,  5000.0f, 0.30f,  64.0f,  3000.0f, 0.90f, kBedDustStorm   },
    };
    constexpr int kStormTypeTableSize = static_cast<int>(std::size(kStormTypes));

    constexpr int   kAshStormIndex     = 0;
    constexpr int   kDustStormIndex    = 4;
    constexpr int   kDustFogRollTicks  = 40;
    constexpr float kDustBurstSec      = 4.0f;
    constexpr float kWarningBuildupMaxPct = 0.35f;

    constexpr float kDustFogThinNear  = 8000.0f;
    constexpr float kDustFogThinFar   = 40000.0f;
    constexpr float kDustFogThinPow   = 1.0f;

    inline const char* PerkName(RE::BGSPerk* perk)
    {
        if (!perk) return "<null>";
        const char* n = perk->GetFullName();
        return (n && n[0]) ? n : "<unnamed>";
    }

    inline void ApplyWeatherFog(RE::TESWeather* w, float fNear, float fFar, float fPow,
                                float hMid, float hRange, float hiDens)
    {
        if (!w) return;
        w->fogData[0]  = w->fogData[2]  = fNear;
        w->fogData[1]  = w->fogData[3]  = fFar;
        w->fogData[4]  = w->fogData[5]  = fPow;
        w->fogData[8]  = w->fogData[10] = w->fogData[14] = w->fogData[16] = hMid;
        w->fogData[9]  = w->fogData[11] = w->fogData[15] = w->fogData[17] = hRange;
        w->fogData[12] = w->fogData[13] = hiDens;
    }

    constexpr float kStormShakeBurstStrength     = 0.75f;
    constexpr int   kStormShakeBurstSteps        = 5;
    constexpr float kStormShakeBurstDuration     = 0.4f;
    constexpr float kStormShakeRumbleStrength    = 0.12f;
    constexpr float kStormShakeRumbleDuration    = 0.7f;
    constexpr int   kStormShakeRumbleTicks       = 2;
    constexpr int   kStormShakeBurstMaxRetries   = 20;

    constexpr float kDustMoteSec          = 5.0f;
    constexpr int   kDustMoteReapplyTicks = 18;

    constexpr float kShelterHeadZ         = 96.0f;
    constexpr float kRoofRayDist          = 768.0f;
    constexpr float kRoofFanHoriz         = 0.342f;
    constexpr float kRoofFanVert          = 0.940f;
    constexpr int   kRoofFanMinHits       = 3;
    constexpr int   kEnclosedRecheckTicks = 4;

    constexpr std::size_t kPowerArmorFullPieces = 6;

    inline bool RayHitsStructure(RE::TESObjectCELL* cell, const RE::NiPoint3& from, const RE::NiPoint3& to)
    {
        if (!cell) return false;
        RE::bhkPickData pick;
        pick.SetStartEnd(from, to);
        pick.allowFailedPicks = false;
        return cell->Pick(pick) != nullptr || pick.HasHit();
    }
}
