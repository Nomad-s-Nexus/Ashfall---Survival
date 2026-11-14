#include "RadEngine_Internal.h"
#include "RadCorrectnessPolicy.h"
#include "RadMaskOwnership.h"
#include "RadNeedsTuning.h"

#include <RE/I/IMenu.h>
#include <RE/U/UI.h>

#include <algorithm>
#include <vector>

static_assert(static_cast<int>(StormPhase::Idle) == RadCorrectnessPolicy::kPhaseIdle &&
              static_cast<int>(StormPhase::Warning) == RadCorrectnessPolicy::kPhaseWarning &&
              static_cast<int>(StormPhase::Active) == RadCorrectnessPolicy::kPhaseActive &&
              static_cast<int>(StormPhase::Aftermath) == RadCorrectnessPolicy::kPhaseAftermath &&
              static_cast<int>(StormPhase::Approach) == RadCorrectnessPolicy::kPhaseApproach,
              "StormPhase values must match RadCorrectnessPolicy phase constants");

using namespace RadInternal;


namespace
{
    using RadCorrectnessPolicy::kAutoMaskStableTicks;

    bool GameWindowForeground()
    {
        const HWND foreground = ::GetForegroundWindow();
        if (!foreground)
            return false;
        DWORD pid = 0;
        ::GetWindowThreadProcessId(foreground, &pid);
        return pid == ::GetCurrentProcessId();
    }

    bool BlockingMenuOpen()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui)
            return false;
        RE::BSAutoReadLock lock{ RE::UI::GetMenuMapRWLock() };
        for (const auto& menu : ui->menuStack)
        {
            if (!menu || !menu->OnStack())
                continue;
            if (menu->menuFlags.all(RE::UI_MENU_FLAGS::kPausesGame) ||
                menu->menuFlags.all(RE::UI_MENU_FLAGS::kUsesCursor) ||
                menu->menuFlags.all(RE::UI_MENU_FLAGS::kModal) ||
                menu->menuFlags.all(RE::UI_MENU_FLAGS::kFreezeFramePause))
                return true;
        }
        return false;
    }
    class MaskHitSink : public RE::BSTEventSink<RE::TESHitEvent>
    {
    public:
        static MaskHitSink* GetSingleton() { static MaskHitSink s; return &s; }
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& a_event,
                                              RE::BSTEventSource<RE::TESHitEvent>*) override
        {
            if (a_event.target && a_event.target->IsPlayerRef())
                RadiationEngine::GetSingleton().OnPlayerHit();
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}


bool RadiationEngine::Initialize()
{
    auto logger = Log();

    static_assert(kStormTypeCount == kStormTypeTableSize,
                  "kStormTypeCount must match the kStormTypes table length");
    static_assert(RadCorrectnessPolicy::kStormSkyTypeCount == kStormTypeCount &&
                  RadCorrectnessPolicy::kStormSkyDustType == kDustStormIndex,
                  "storm sky policy constants must match the storm table");

    logger->info("========== RADIATION ENGINE INITIALIZATION ==========");

    try
    {
        logger->debug("[1] Getting RadiationConfig singleton...");
        _config = &RadiationConfig::GetSingleton();
        if (!_config)
        {
            logger->error("  ✗ Failed to get config singleton");
            return false;
        }
        logger->debug("  ✓ RadiationConfig singleton retrieved");

        auto dataPath = "Data/F4SE/Plugins/FalloutAnomaly";
        logger->debug("[2] Loading configuration from: {}", dataPath);

        if (!std::filesystem::exists(dataPath))
        {
            logger->error("  ✗ Config path does not exist: {}", dataPath);
            logger->error("  Create the folder and add thresholds.json.");
            return false;
        }
        logger->debug("  ✓ Config directory exists");

        if (!_config->LoadFromFile(dataPath))
        {
            logger->error("✗ Failed to load config from {}", dataPath);
            logger->error("  Check logs above for specific parsing errors");
            return false;
        }

        logger->info("  ✓ Configuration loaded successfully");
        logger->debug("    - Radiation stage thresholds loaded (thresholds.json)");

        _currentStageLevel = 0;
        _lastUpdateTime = std::chrono::high_resolution_clock::now();

        LoadFilterIni();

        static bool s_hitSinkRegistered = false;
        if (!s_hitSinkRegistered)
        {
            if (auto* src = RE::TESHitEvent::GetEventSource())
            {
                src->RegisterSink(MaskHitSink::GetSingleton());
                s_hitSinkRegistered = true;
                logger->info("[Condition] Player-hit sink registered (mask takes damage when struck).");
            }
        }

        logger->info("✓ RadiationEngine initialized successfully");
        logger->info("====================================================");
        return true;
    }
    catch (const std::exception& ex)
    {
        logger->error("✗ Exception during RadiationEngine initialization: {}", ex.what());
        logger->error("====================================================");
        return false;
    }
}


void RadiationEngine::Update()
{
    auto logger = Log();
    RadHud::GetSingleton().TickLoadSettle();
    RadHud::GetSingleton().PumpDomReady();
    RadHud::GetSingleton().SyncDebugPanelState();
    RadHud::GetSingleton().ReleaseStaleDebugFocus();

    try
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            logger->debug("[Update] Player singleton not available (game not loaded)");
            return;
        }

        auto* actorValue = RE::ActorValue::GetSingleton();
        if (!actorValue || !actorValue->rads)
        {
            logger->debug("[Update] ActorValue singleton or rads value not available");
            return;
        }

        _playSeconds += 0.25;

        EnsureEslLoaded();
        if (_pendingLoadFXCleanup)
        {
            _pendingLoadFXCleanup = false;
            TriggerStormFX(false);
            ClearStormIMAD();
        }
        DrainDebugCommands();
        if (_debugGod) KeepPlayerAlive();

        RE::Sky* sky = RE::Sky::GetSingleton();
        const bool vanillaRadstorm = sky && sky->currentWeather &&
            IsRadstormWeather(sky->currentWeather->GetFormID());
        const bool stormActive = (_stormPhase != StormPhase::Idle) || vanillaRadstorm;

        const bool rainyWeather = sky && sky->currentWeather && IsRainyWeather(sky->currentWeather);

        bool inRadWeather = false;
        if (_radWeatherList && sky && sky->currentWeather)
            inRadWeather = _radWeatherList->ContainsItem(sky->currentWeather);
        const std::uint32_t currentWeatherID = sky && sky->currentWeather
            ? sky->currentWeather->GetFormID() : 0;
        bool enteredRadWeather = false;

        if (inRadWeather && _dustBuildupActive && _stormPhase == StormPhase::Warning)
            inRadWeather = false;

        const bool needsShelterEval = stormActive || rainyWeather || inRadWeather;

        if (!needsShelterEval && !_debugShelter)
        {
            _playerEnclosed    = false;
            _enclosedCheckTick = 0;
        }
        else if (--_enclosedCheckTick <= 0)
        {
            _enclosedCheckTick = kEnclosedRecheckTicks;
            _playerEnclosed    = ComputeEnclosedByStructure();
        }

        RetryStormGhoulCleanup();
        RetryPendingStormGhouls();
        RetireUnresolvedStormGhouls();

        if (--_ghoulCleanupTick <= 0)
        {
            _ghoulCleanupTick = kGhoulCleanupTicks;
            CheckGhoulPackCleanup();
        }

        float particulateRads = player->GetActorValue(*actorValue->rads);
        float currentHP = actorValue->health ? player->GetActorValue(*actorValue->health) : 0.0f;

        const bool inPowerArmor = IsPlayerInPowerArmor();
        bool blockRadGain = needsShelterEval &&
            (IsPlayerInPoweredShelter() || inPowerArmor || IsPlayerSheltered());
        if (_spawnRadBlockTicks > 0) { blockRadGain = true; --_spawnRadBlockTicks; }
        if (_prevRads >= 0.0f && particulateRads > _prevRads + 0.001f && blockRadGain) {
            const float gain = particulateRads - _prevRads;
            player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *actorValue->rads, -gain);
            particulateRads = _prevRads;
        }

        const float nowDays         = GameDays();
        const float gameDaysElapsed = (_prevGameDays >= 0.0f) ? (nowDays - _prevGameDays) : 0.0f;
        const bool  timeJumped      = (_prevGameDays >= 0.0f) && (gameDaysElapsed > kCureGapDays);
        _prevGameDays = nowDays;
        CheckActiveCures(particulateRads, currentHP, !timeJumped);
        _prevRads = particulateRads;
        _prevHP   = currentHP;
        if (timeJumped)
            ApplyRestTissueHeal(gameDaysElapsed * 24.0f, particulateRads);

        float effectiveRads = UpdateTissueDamage(particulateRads);

        static int updateCount = 0;
        if (++updateCount % 32 == 0)
        {
            const char* mode = _eslAvailable ? "ESL-perks" : "C++-fallback";
            logger->debug("[Status] particulate={:.1f} tissue={:.1f} effective={:.1f} stage={} mode={} perkIdx={}",
                particulateRads, _tissueDamage, effectiveRads, _currentStageLevel, mode, _appliedPerkIndex);
        }

        CheckRadstormWarning();

        UpdateStorm();

        UpdateDustFogRoll();

        UpdateDustMotes();

        UpdateAmbientAudio();

        RadHud::GetSingleton().PushToxic(_tissueDamage, _permanentTissueDamage);

        ApplyTissueEffects();

        const bool realMasked = IsWearingGasMask();
        const bool maskOn = RadCorrectnessPolicy::MaskOverlayVisible(
            _maskOverride, realMasked, PlayerWearsPowerArmorHelmet(), PlayerInFirstPersonView());

        const int maskNow = realMasked ? 1 : 0;
        if (_maskWasOn == -1)
            _maskWasOn = maskNow;
        else if (maskNow != _maskWasOn)
        {
            {
                const bool female = IsPlayerFemale();
                RE::BSSoundHandle h{};
                Play2DSound(realMasked ? (female ? kSndMaskDrawF : kSndMaskDrawM)
                                       : (female ? kSndMaskOffF : kSndMaskOffM), h);
            }
            _maskWasOn = maskNow;
        }

        RadHud::GetSingleton().PushMask(maskOn);

        const bool filterSys = (_filterItem != nullptr) && _filterEnabled;
        const bool condSys   = _conditionEnabled;
        if (filterSys || condSys)
        {
            const bool maskExposed = !IsPlayerSheltered() && !IsPlayerInPoweredShelter() &&
                                     !IsPlayerInPowerArmor();
            if (realMasked)
            {
                if (filterSys && maskExposed && inRadWeather && _filterCharge > 0.0f)
                    _filterCharge = std::max(0.0f, _filterCharge - (100.0f / _filterDurationSec) * 0.25f);

                if (filterSys && _filterCharge <= 0.0f && _filterAutoReplace && FilterItemCount() > 0)
                    ReplaceFilter(true);
                if (condSys && _maskCondition <= 0.0f && _conditionAutoRepair && RepairKitCount() > 0)
                    RepairMask(true);

                const bool filterDead = filterSys && _filterCharge <= 0.0f;
                const bool maskBroken = condSys && _maskCondition <= 0.0f;

                if ((filterDead || maskBroken) && maskExposed && inRadWeather && actorValue->rads)
                    player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *actorValue->rads,
                                          _filterChokeRads * 0.25f);

                if (filterSys)
                {
                    if (filterDead) { if (_filterLowWarned < 2) { Notify("[Mask] Filter DEPLETED - replace it!", true); _filterLowWarned = 2; } }
                    else if (_filterCharge <= 20.0f && _filterLowWarned < 1) { Notify("[Mask] Filter running low.", true); _filterLowWarned = 1; }
                }
                if (condSys)
                {
                    if (maskBroken) { if (_conditionLowWarned < 2) { Notify("[Mask] Mask BROKEN - repair it!", true); _conditionLowWarned = 2; } }
                    else if (_maskCondition <= 25.0f && _conditionLowWarned < 1) { Notify("[Mask] Mask is damaged.", true); _conditionLowWarned = 1; }
                }
            }
            if (realMasked)
            {
                if (--_filterCountTick <= 0) { _filterSparesCache = FilterItemCount(); _filterCountTick = 8; }
            }
            else { _filterCountTick = 0; }
            RadHud::GetSingleton().PushFilter(filterSys ? _filterCharge : -1.0f,
                                              condSys ? _maskCondition : -1.0f,
                                              realMasked ? _filterSparesCache : 0, realMasked);
        }
        else
        {
            RadHud::GetSingleton().PushFilter(-1.0f, -1.0f, 0, realMasked);
        }

        const bool airSheltered = IsPlayerSheltered() || IsPlayerInPoweredShelter();
        const bool airSealed = IsPlayerInPowerArmor();
        const bool airExposed = inRadWeather && !airSheltered && !airSealed;
        const bool failedAirMask = realMasked &&
            ((filterSys && _filterCharge <= 0.0f) || (condSys && _maskCondition <= 0.0f));
        const float airHealthLoss = _chokingEnabled && airExposed && (!realMasked || failedAirMask)
            ? _chokeHealthPerSec * (realMasked ? _chokeFailedFactor : 1.0f) : 0.0f;
        const auto* airPlayer = RE::PlayerCharacter::GetSingleton();
        const int   airAutoMask = !RadNeedsTuning::On(RadNeedsTuningPolicy::kAutoEquipMask, true) ? 0
            : (!RadNeedsTuning::On(RadNeedsTuningPolicy::kAutoEquipInCombat, true) && airPlayer && airPlayer->IsInCombat()) ? 2 : 1;
        RadHud::GetSingleton().PushAir(json{
            { "contaminated", inRadWeather }, { "sheltered", airSheltered }, { "sealed", airSealed },
            { "masked", realMasked }, { "failed", failedAirMask }, { "chokingEnabled", _chokingEnabled },
            { "healthLoss", actorValue->health ? airHealthLoss : 0.0f },
            { "radGain", airExposed && failedAirMask && actorValue->rads ? _filterChokeRads : 0.0f },
            { "filterDrain", airExposed && realMasked && filterSys && _filterCharge > 0.0f
                ? 100.0f / _filterDurationSec : 0.0f },
            { "charge", filterSys ? std::round(_filterCharge) : -1.0f },
            { "condition", condSys ? std::round(_maskCondition) : -1.0f },
            { "spares", realMasked ? _filterSparesCache : 0 },
            { "autoMask", airAutoMask }
        }.dump());

        if (_debugShelter && --_chokeDiagTick <= 0)
        {
            _chokeDiagTick = 4;
            const std::uint32_t wid = (sky && sky->currentWeather) ? sky->currentWeather->GetFormID() : 0;
            logger->info("[ChokeDiag] weather=0x{:08X} inRadWx={} stormActive={} | realMasked={} maskOverride={} "
                         "| enclosed={} sheltered={} PA={} poweredShelter={} | filterCharge={:.0f} cond={:.0f} "
                         "chokingEnabled={} chokeTicks={} calmTicks={}",
                         wid, inRadWeather, stormActive, IsWearingGasMask(), _maskOverride,
                         _playerEnclosed, IsPlayerSheltered(), IsPlayerInPowerArmor(), IsPlayerInPoweredShelter(),
                         _filterCharge, _maskCondition, _chokingEnabled, _chokeTicks, _chokeCalmTicks);
        }

        if (_chokingEnabled && inRadWeather && actorValue->health)
        {
            const bool exposed = !IsPlayerSheltered() && !IsPlayerInPoweredShelter() && !IsPlayerInPowerArmor();
            const bool filterDead = (_filterItem != nullptr) && _filterEnabled && _filterCharge <= 0.0f;
            const bool maskBroken = _conditionEnabled && _maskCondition <= 0.0f;
            const bool failedMask = realMasked && (filterDead || maskBroken);
            const bool choking    = exposed && (!realMasked || failedMask);

            if (choking)
            {
                _chokeCalmTicks = 0;
                if (_chokeTicks < 1000) ++_chokeTicks;

            const float rate = airHealthLoss;
                player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *actorValue->health,
                                      GodModeHealthDrain(player, actorValue->health, -rate * 0.25f));

                if (_chokeTicks >= RadCorrectnessPolicy::kChokeGraceTicks)
                {
                    if (!_chokeNotified)
                    {
                        Notify(realMasked ? "[Mask] Your mask is FAILING - you're choking!"
                                          : "You're breathing contaminated air - mask up!", true);
                        _chokeNotified = true;
                    }
                    if (_chokeSounds && --_chokeSoundTick <= 0)
                    {
                        const bool female = IsPlayerFemale();
                        RE::BSSoundHandle h{};
                        Play2DSound(realMasked ? (female ? kSndChokeWithMaskF : kSndChokeWithMaskM)
                                               : (female ? kSndChokeNoMaskF : kSndChokeNoMaskM), h);
                        static std::mt19937 rng{ std::random_device{}() };
                        _chokeSoundTick = std::uniform_int_distribution<int>(24, 44)(rng);
                    }
                }
            }
            else if ((_chokeTicks > 0 || _chokeNotified) && ++_chokeCalmTicks >= 40)
            {
                _chokeTicks = 0; _chokeCalmTicks = 0;
                _chokeNotified = false; _chokeSoundTick = 0;
            }
        }
        else if ((_chokeTicks > 0 || _chokeNotified) && ++_chokeCalmTicks >= 40)
        {
            _chokeTicks = 0; _chokeCalmTicks = 0;
            _chokeNotified = false; _chokeSoundTick = 0;
        }

        if (_radWeatherList)
        {
            const int inRadNow = inRadWeather ? 1 : 0;
            if (_wasInRadWeather == -1)
            {
                _wasInRadWeather = inRadNow;
                _lastRadWeatherFormID = inRadWeather ? currentWeatherID : 0;
            }
            else if (inRadNow != _wasInRadWeather)
            {
                enteredRadWeather = inRadWeather;
                if (inRadWeather)
                {
                    EquipNearbyNpcMasks(true);
                    _npcMaskScanTick = kNpcMaskRescanTicks;
                }
                else
                {
                    EquipNearbyNpcMasks(false);
                }
                _wasInRadWeather = inRadNow;
            }
            else if (inRadWeather && _lastRadWeatherFormID != 0 &&
                     _lastRadWeatherFormID != currentWeatherID)
            {
                enteredRadWeather = true;
            }
            else if (inRadWeather && --_npcMaskScanTick <= 0)
            {
                _npcMaskScanTick = kNpcMaskRescanTicks;
                EquipNearbyNpcMasks(true);
            }
            _lastRadWeatherFormID = inRadWeather ? currentWeatherID : 0;
        }

        if (auto* cell = player->GetParentCell())
        {
            const bool exposedNow = RadCorrectnessPolicy::RadWeatherExposed(
                inRadWeather,
                cell->IsInterior(),
                IsPlayerSheltered(),
                IsPlayerInPoweredShelter(),
                inPowerArmor);
            const auto step = RadCorrectnessPolicy::StepAutoMask(
                _wasRadExposed,
                _radExposedCandidate,
                _radExposedStreak,
                exposedNow,
                kAutoMaskStableTicks);
            _wasRadExposed = step.applied;
            _radExposedCandidate = step.candidate;
            _radExposedStreak = step.streak;
            const bool donAllowed = RadNeedsTuning::On(RadNeedsTuningPolicy::kAutoEquipMask, true) &&
                (RadNeedsTuning::On(RadNeedsTuningPolicy::kAutoEquipInCombat, true) || !player->IsInCombat());
            if (step.equip && !donAllowed)
                _wasRadExposed = 0;
            else if (step.equip && !IsWearingGasMask())
            {
                if (auto* mask = FindInventoryMask())
                {
                    SetMaskEquipped(mask, true);
                    _autoMaskEquipped = true;
                }
            }
            else if (step.remove && _autoMaskEquipped)
            {
                if (auto* worn = GetWornMask())
                    SetMaskEquipped(worn, false);
                _autoMaskEquipped = false;
            }
        }
        if (RadCorrectnessPolicy::SpawnGhoulsOnRadWeatherEntry(enteredRadWeather, vanillaRadstorm,
                                                               _debugWeatherHold, static_cast<int>(_stormPhase)))
            SpawnStormGhouls();

        ApplyDebuffs(static_cast<uint32_t>(effectiveRads));

    }
    catch (const std::exception& ex)
    {
        logger->error("[Update] Exception: {}", ex.what());
    }
}


std::uint32_t RadiationEngine::ReadInputBits() const noexcept
{
    const int keys[4] = { kDebugMenuKey, VK_LSHIFT, _filterReplaceKey.load(), _conditionRepairKey.load() };
    std::uint32_t bits = 0;
    for (std::size_t i = 0; i < 4; ++i)
    {
        if (keys[i] != 0 && (GetAsyncKeyState(keys[i]) & 0x8000) != 0)
            bits |= 1u << i;
    }
    return bits;
}


void RadiationEngine::ResetDebugKeyBaseline() noexcept
{
    _f8WasDown = (ReadInputBits() & 1u) != 0;
}

void RadiationEngine::PollInput()
{
    _polledInputBits.store(ReadInputBits());
    const std::uint32_t bits = _polledInputBits.load();
    const bool dbgKeyDown = (bits & (1u << 0)) != 0;
    const bool lshift     = (bits & (1u << 1)) != 0;
    const bool debugToggleAllowed = GameWindowForeground() || RadHud::GetSingleton().DebugMenuOpen();
    if (dbgKeyDown && !_f8WasDown && lshift && debugToggleAllowed)
    {
        ReloadHotkeysFromIni();
        RadHud::GetSingleton().ToggleDebugMenu();
    }
    _f8WasDown = dbgKeyDown;

    RadHud::GetSingleton().PumpDeferredDebug();

    if (_filterReplaceKey == 0 && _conditionRepairKey == 0)
    {
        _filterReplaceWasDown = false;
        _conditionRepairWasDown = false;
        return;
    }
    const bool hotkeysAllowed = GameWindowForeground() && !BlockingMenuOpen();

    if (_filterReplaceKey != 0)
    {
        const bool rdown = (bits & (1u << 2)) != 0;
        if (hotkeysAllowed && rdown && !_filterReplaceWasDown)
            ReplaceFilter(false);
        _filterReplaceWasDown = rdown;
    }
    else
    {
        _filterReplaceWasDown = false;
    }

    if (_conditionRepairKey != 0)
    {
        const bool rdown = (bits & (1u << 3)) != 0;
        if (hotkeysAllowed && rdown && !_conditionRepairWasDown)
            RepairMask(false);
        _conditionRepairWasDown = rdown;
    }
    else
    {
        _conditionRepairWasDown = false;
    }
}
