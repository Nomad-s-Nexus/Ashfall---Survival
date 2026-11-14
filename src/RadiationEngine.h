#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <RE/B/BSPointerHandle.h>
#include <RE/B/BSSoundHandle.h>
#include <RE/I/ImageSpaceModifierInstanceForm.h>
#include <RE/N/NiPoint3.h>
#include <RE/N/NiPointer.h>
#include <RE/B/BSGeometry.h>

#include "RadCorrectnessPolicy.h"

class RadiationConfig;
namespace F4SE { class SerializationInterface; }
namespace RE {
    class NiAVObject;
    class Actor;
    class ActorValueInfo;
    class PlayerCharacter;
    class BGSHazard;
    class BGSExplosion;
    class BGSArtObject;
    class BGSKeyword;
    class BGSLocation;
    class BGSPerk;
    class TESImageSpaceModifier;
    class TESGlobal;
    class TESObjectCELL;
    class TESObjectREFR;
    class TESObjectARMO;
    class TESBoundObject;
    class TESWeather;
    class TESQuest;
    class TESWorldSpace;
    class BGSListForm;
}

enum class StormPhase
{
    Idle,
    Warning,
    Active,
    Aftermath,
    Approach
};

class RadiationEngine
{
public:
    static RadiationEngine& GetSingleton()
    {
        static RadiationEngine instance;
        return instance;
    }

    bool Initialize();
    void Update();
    void PollInput();
    void UpdateStormIMADFade();
    void ApplyDebuffs(uint32_t radLevel);

    void ResetAppliedPenalties()
    {
        _appliedEndPenalty = 0;
        _appliedStrPenalty = 0;
        _specialSuspendStage = -1;
        _currentStageLevel = 0;
        _appliedPerkIndex = -1;
        _activeStormImadIndex = -1;
        _stormImadInstance = nullptr;
        _stormImadTargetStrength = 0.0f;
        _stormImadFadeRate = 0.0f;
        _stormImadStopWhenFaded = false;
        _radstormWarned = false;
        if (_radstormCueHandle.IsValid()) _radstormCueHandle.FadeOutAndRelease(500);
        _stormRollInTicks = 0;
        _prevRads = -1.0f;
        _prevHP   = -1.0f;
        _radAwayWindowTicks = 0;
    }

    void OnPostLoad();

    void OnNewGame();

    void SaveState(const F4SE::SerializationInterface* a_intfc) const;
    void LoadState(const F4SE::SerializationInterface* a_intfc);
    void RevertState();

    void DebugForceStorm(int typeIndex);
    void DebugEndStorm();
    void DebugAddTissue(float tissue, float permanent);
    void DebugAddRads(float rads);
    void DebugClearAll();
    void DebugGiveItem(uint32_t formID, const char* plugin, int count);
    void DebugPreviewWeather(int idx);
    void DebugForceWeatherByForm(const std::string& spec);
    void DebugScanWornMask();
    void DebugClearWeather();
    void PushGhoulSettingsToHud();
    void DebugHeal();
    void DebugTime(const std::string& arg);
    void ResumeDebugTime();
    void DrainDebugCommands();
    void DebugApplyNeeds(const std::string& arg);
    void DebugOutbreak(const std::string& arg);
    void DebugLockWeather();
    void KeepPlayerAlive();
    void DebugFixSpecial();
    void WantedTissuePenalties(float (&a_out)[4]) const;
    void RepairLeakedCarryWeight();
    void DispelNegativeSpecials();
    void HandleDebugCommand(const char* action);
    [[nodiscard]] bool StormGhoulsEnabled() const noexcept { return _stormGhoulsEnabled; }
    [[nodiscard]] int FilterReplaceKey() const noexcept { return _filterReplaceKey; }
    [[nodiscard]] int ConditionRepairKey() const noexcept { return _conditionRepairKey; }
    [[nodiscard]] std::uint32_t ReadInputBits() const noexcept;
    [[nodiscard]] std::uint32_t PolledInputBits() const noexcept { return _polledInputBits.load(); }
    void ResetDebugKeyBaseline() noexcept;

    void OnPlayerHit();

    void NotifyVerifiedCureUse(RadCorrectnessPolicy::CureSignal signal);

    [[nodiscard]] float PlayerHealthDrain(RE::PlayerCharacter* player, RE::ActorValueInfo* healthAv, float kDamage) const
    {
        return GodModeHealthDrain(player, healthAv, kDamage);
    }

    [[nodiscard]] bool PlayerGodMode(RE::PlayerCharacter* player) const;

private:
    RadiationEngine() = default;
    ~RadiationEngine() = default;

    void DispatchDebugCommand(const std::string& action);
    std::mutex               _debugCmdMutex;
    std::vector<std::string> _pendingDebugCmds;

    RadiationConfig* _config = nullptr;
    uint32_t _currentStageLevel = 0;
    std::chrono::high_resolution_clock::time_point _lastUpdateTime;

    int32_t _appliedEndPenalty = 0;
    int32_t _appliedStrPenalty = 0;

    int32_t _specialSuspendStage = -1;

    static constexpr int kStageCount = 10;
    struct StagePenalty { int end; int str; };
    static constexpr StagePenalty kStagePenalties[kStageCount] = {
        {1, 0}, {1, 1}, {2, 1}, {2, 2}, {3, 2}, {3, 3}, {4, 3}, {5, 4}, {6, 4}, {7, 5},
    };
    RE::BGSPerk* _stagePerks[kStageCount] = {};
    int _appliedPerkIndex = -1;
    bool _eslAvailable = false;
    bool _eslChecked = false;

    RE::TESQuest* _stormFXQuest = nullptr;
    void TriggerStormFX(bool on);
    int  _sirenDelayTicks = 0;
    static void EnsureQuestRunning(RE::TESQuest* quest);
    static void DumpQuestStageTable(RE::TESQuest* quest);

    RE::TESGlobal* _tissueGlobal = nullptr;
    float _tissueDamage = 0.0f;
    float UpdateTissueDamage(float particulateRads);

    RE::TESGlobal* _permanentTissueGlobal = nullptr;
    float _permanentTissueDamage = 0.0f;
    void HealPermanentTissue(float amount, const char* source);

    float _prevRads = -1.0f;
    float _prevHP   = -1.0f;
    float _prevGameDays = -1.0f;

    bool _f8WasDown = false;
    std::atomic<std::uint32_t> _polledInputBits{0};

    static constexpr int kStormTypeCount = 5;
    RE::TESWeather* _stormWeathers[kStormTypeCount] = {};
    RE::TESWeather* _stormSkyVanilla = nullptr;
    RE::TESWeather* StormSkyWeather(int typeIndex, RadCorrectnessPolicy::StormSkyKind kind) const;
    void ResetStormWeatherLatches();
    RE::TESGlobal*  _nextEmissionGlobal = nullptr;
    RE::TESGlobal*  _forceStormGlobal = nullptr;
    StormPhase      _stormPhase = StormPhase::Idle;
    int             _activeStormType = -1;
    int             _dustFogRollTick = 0;
    bool            _dustBuildupActive = false;
    int             _warnTicksTotal = 0;
    RE::TESWeather* _dustRollPrevWeather = nullptr;
    int             _dustRollElapsed = 0;
    int             _dustRollTotal   = 0;
    RE::BGSArtObject* _dustBurstArt = nullptr;
    int             _forcedStormType = -1;
    bool            _forceStormNow = false;
    bool            _stormScheduled = false;
    double          _playSeconds      = 0.0;
    double          _nextStormPlaySec = 0.0;
    int             _stormTicksLeft = 0;
    int             _stormExposedTicks = 0;
    bool            _weatherForced = false;
    RE::TESWeather* _weatherReturnTarget = nullptr;
    bool            _weatherReturnCaptured = false;
    bool            _releaseWeatherOverrideWhenSettled = false;
    int             _weatherHandoffSettleTicks = 0;
    int             _weatherHandoffWaitTicks = 0;
    bool            _debugWeatherHold = false;
    int             _stormPeakTick = 0;
    bool            _stormPeakFired = false;
    int             _stormRollInTicks = 0;
    bool            _stormFXVisible = false;
    bool            _stormNukeFired = false;
    bool            _firestormAwaitingNuke = false;
    int             _nukeAttempts = 0;
    int             _nukeWaitTicks = 0;
    int             _shakeBurstRetries = 0;
    bool            _debugSkipToActive = false;
    RE::BSSoundHandle _stormSoundHandle{};
    RE::BSSoundHandle _radstormCueHandle{};
    RE::BSSoundHandle _ambientBedHandle{};
    const char*     _curStormBed   = nullptr;
    const char*     _curAmbientBed = nullptr;
    bool            _ambientEnabled = true;
    std::uint16_t   _ambientAtten   = 1500;
    std::uint16_t   _stormBedAtten  = 500;

    bool            _stormsEnabled     = true;
    float           _stormMinRealMin   = 35.0f;
    float           _stormMaxRealMin   = 90.0f;
    float           _stormGapAppliedMin = 0.0f;
    float           _stormGapAppliedMax = 0.0f;
    std::uint16_t   _stormBedApplied   = 0xFFFF;
    std::uint16_t   _ambientBedApplied = 0xFFFF;
    double          _vanillaRadstormGapSec = 60.0 * 60.0;
    int             _stormLeadTicks    = 3 * 60 * 4;
    int             _forcedLeadTicks   = -1;
    int             _activeLeadTicks   = 0;
    bool _pendingLoadFXCleanup = false;

    void ForceClearAndSilence();

    RE::TESWeather* _clearWeather       = nullptr;
    bool            _forceClearOnEntry  = false;
    int             _spawnRadBlockTicks = 0;

    std::unordered_set<std::uint32_t> _dustWeathers;

    void UpdateStorm();
    void BeginSmoothWeather(RE::TESWeather* a_target, bool a_lockOverride = true);
    void UpdateDustFogRoll();
    void PlayStormCue();
    void StopStormCue(std::uint16_t fadeMs);
    void BeginStorm(int typeIndex);
    void EnterWarningPhase();
    void EnterStormPhase(StormPhase phase, int durationTicks);
    void EffectiveStormGap(float& a_min, float& a_max) const;
    void ScheduleNextStorm();
    float GameDays() const;
    int  RollStormType() const;
    bool IsPlayerSheltered() const;
    bool IsPlayerInPoweredShelter() const;
    bool IsPlayerInPowerArmor() const;
    bool PlayerInFirstPersonView() const;
    bool PlayerWearsPowerArmorHelmet() const;
    bool IsActorInPowerArmor(RE::Actor* actor) const;
    RE::BGSKeyword* _armorTypePowerKeyword = nullptr;
    bool IsRecognizedGasMask(const RE::TESObjectARMO* armor) const;
    RE::TESObjectARMO* GetActorWornMask(RE::Actor* actor) const;
    RE::TESObjectARMO* GetWornMask() const;
    bool IsWearingGasMask() const;
    RE::BGSKeyword* _gasMaskKeyword = nullptr;
    int  _maskOverride = -1;
    std::vector<RE::BGSKeyword*> _extraMaskKeywords;
    std::vector<const RE::TESObjectARMO*> _extraMasks;
    bool _inferMasksFromRadResist = true;
    int _minMaskRadResist = 10;
    bool IsListedMask(const RE::TESObjectARMO* armor) const;
    bool HasRadResistance(const RE::TESObjectARMO* armor) const;

    RE::TESObjectARMO* FindActorInventoryMask(RE::Actor* actor) const;
    RE::TESObjectARMO* FindInventoryMask() const;
    void SetActorMaskEquipped(RE::Actor* actor, RE::TESObjectARMO* mask, bool equip);
    void SetMaskEquipped(RE::TESObjectARMO* mask, bool equip);
    void EquipNearbyNpcMasks(bool equip);
    RE::BGSListForm* _radWeatherList = nullptr;
    int  _wasInRadWeather   = -1;
    int  _wasRadExposed     = -1;
    int  _radExposedCandidate = -1;
    int  _radExposedStreak    = 0;
    std::uint32_t _lastRadWeatherFormID = 0;
    bool _autoMaskEquipped  = false;
    int  _npcMaskScanTick   = 0;
    std::unordered_set<std::uint32_t> _autoMaskedNpcs;

    float _filterCharge      = 100.0f;
    int   _filterDurationSec = 1200;
    std::atomic<int> _filterReplaceKey{0};
    bool  _filterAutoReplace = false;
    float _filterChokeRads   = 8.0f;
    RE::TESBoundObject* _filterItem = nullptr;
    bool  _filterReplaceWasDown = false;
    int   _filterLowWarned   = 0;
    int   _filterSparesCache = 0;
    int   _filterCountTick   = 0;
    bool  _filterEnabled     = true;
    void  LoadFilterIni();
    void  ApplyTuning();
    void  ReloadHotkeysFromIni();
    std::uint64_t _hotkeyIniWriteTime = 0;
    void  LoadMaskConfig();
    void  ReplaceFilter(bool silent);
    int   FilterItemCount() const;

    float _maskCondition       = 100.0f;
    bool  _conditionEnabled    = true;
    float _conditionHitDamage  = 6.0f;
    std::atomic<int> _conditionRepairKey{0};
    bool  _conditionAutoRepair = false;
    RE::TESBoundObject* _repairKitItem = nullptr;
    bool  _conditionRepairWasDown = false;
    int   _conditionLowWarned  = 0;
    void  RepairMask(bool silent);
    int   RepairKitCount() const;
    float GodModeHealthDrain(RE::PlayerCharacter* player, RE::ActorValueInfo* healthAv, float kDamage) const;
    bool  _respectGodMode    = true;
    bool  _chokingEnabled    = true;
    float _chokeHealthPerSec = 1.5f;
    float _chokeFailedFactor = 0.5f;
    bool  _chokeSounds       = true;
    int   _chokeSoundTick    = 0;
    bool  _chokeNotified     = false;
    int   _chokeTicks        = 0;
    int   _chokeCalmTicks    = 0;
    int   _chokeDiagTick     = 0;

    int   _maskWasOn        = -1;
    bool ComputeEnclosedByStructure() const;
    bool _playerEnclosed    = false;
    int  _enclosedCheckTick = 0;
    bool  _debugGod          = false;
    float _frozenTimeScale   = -1.0f;
    bool _debugShelter      = false;
    void ApplyStormExposure();
    void UpdateAmbientAudio();
    void EnsureBed(RE::BSSoundHandle& handle, const char*& curPath,
                   const char* wantPath, std::uint16_t atten, std::uint16_t& applied);
    void UpdateStormFXVisibility();
    void ApplyStormPeakToNPCs();
    bool IsGhoulActor(RE::Actor* a) const;
    void MakeNearbyNPCsFlee();
    int  _fleeTick = 0;
    void CaptureWeatherReturnTarget();
    void UpdateWeatherHandoff();
    void ReleaseStormWeather();
    void Notify(const char* message, bool warning) const;

    RE::BGSHazard* _fireHazardForm = nullptr;
    RE::BGSExplosion* _nukeExplosionForm = nullptr;
    std::vector<RE::ObjectRefHandle> _fireRefs;
    bool SpawnDistantNuke();
    void SpawnFireRing();
    void ClearFireRefs();

    int  _shakeBurstStep  = -1;
    int  _shakeRumbleTick = -1;
    bool NativeShakeCamera(float strength, float duration);
    void UpdateStormShake();

    int  _dustMoteTick = 0;
    void UpdateDustMotes();

    RE::TESImageSpaceModifier* _stormIMADs[kStormTypeCount] = {};
    int _activeStormImadIndex = -1;
    RE::NiPointer<RE::ImageSpaceModifierInstanceForm> _stormImadInstance;
    float _stormImadTargetStrength = 0.0f;
    float _stormImadFadeRate = 0.0f;
    bool _stormImadStopWhenFaded = false;
    void ApplyStormIMAD(int typeIndex);
    void SetStormIMADTarget(int typeIndex, float strength, float fadeSeconds);
    void FadeOutStormIMAD(float fadeSeconds);
    void ClearStormIMAD();

    static constexpr float kDefaultStormGhoulSpawnChance = 0.40f;
    static constexpr std::uint32_t kStormGhoulPendingTimeoutTicks = 120;
    RE::TESBoundObject* _stormGhoulForm = nullptr;
    RE::TESBoundObject* _stormGlowingOneForm = nullptr;
    std::vector<RE::ObjectRefHandle> _stormGhoulRefs;
    std::vector<RE::ObjectRefHandle> _stormGhoulPendingRefs;
    std::vector<RE::ObjectRefHandle> _stormGhoulCleanupRefs;
    std::uint32_t _stormGhoulPendingTicks = 0;
    std::unordered_map<std::uint32_t, std::uint32_t> _stormGhoulUnresolvedTicksByHandle;
    bool _stormGhoulsEnabled = true;
    float _stormGhoulSpawnChance = kDefaultStormGhoulSpawnChance;
    void RetryPendingStormGhouls();
    void RetryStormGhoulCleanup();
    void RetireUnresolvedStormGhouls();
    void ResetStormGhoulStateForLoad();
    void SpawnStormGhouls();
    void ClearStormGhouls();
    void CheckGhoulPackCleanup();
    RE::NiPoint3 _ghoulPackCenter{};
    std::uint32_t _ghoulPackWorldID = 0;
    bool _ghoulPackInterior = false;
    int  _ghoulCleanupTick = 0;

    RE::TESFaction* _workshopNPCFaction = nullptr;
    RE::BGSKeyword* _ghoulKeyword = nullptr;

    float _lastGameTime = -1.0f;

    void HealTissue(float amount, const char* source);

    void ApplyRestTissueHeal(float gameHoursElapsed, float currentRads);

    void CheckActiveCures(float currentRads, float currentHP, bool deltaValid);
    static constexpr int kRadAwayWindowTicks = 80;
    int _radAwayWindowTicks = 0;

    bool _radstormWarned = false;
    double _lastRadstormSec = -1.0e30;
    bool _prevSheltered      = true;
    int  _cellExitGraceTicks = 0;
    void CheckRadstormWarning();

    void EnsureEslLoaded();
    void UpdateStagePerk(int stageIndex);
    void ApplyStatModification(int stageIndex);
    void RemoveStatModification();
    void ApplyClampedSpecial(RE::PlayerCharacter* player, RE::ActorValueInfo* av,
                             int wantPenalty, std::int32_t& applied);

    void  ApplyTissueEffects();
    void  UpdateTissueSymptoms(float severity01);
    void  ApplyTrackedAV(RE::PlayerCharacter* player, RE::ActorValueInfo* av,
                         float wantPenalty, float& applied);

    float _appliedCarryPen = 0.0f;
    float _appliedHealPen  = 0.0f;
    float _appliedSpeedPen = 0.0f;
    float _appliedApPen    = 0.0f;
    bool  _penaltiesFromSave = false;
    int   _symptomTick     = 0;
    int   _symptomWarned   = 0;
};
