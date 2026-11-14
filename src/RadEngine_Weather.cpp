#include "RadEngine_Internal.h"

using namespace RadInternal;

void RadiationEngine::CheckRadstormWarning()
{
    auto logger = Log();

    if (_maskWasOn == -1)
    {
        _playerEnclosed = false;
        _enclosedCheckTick = 0;
        _prevSheltered = true;
        _cellExitGraceTicks = 0;
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* cell = player ? player->GetParentCell() : nullptr;
        if (cell && cell->IsExterior())
        {
            _playerEnclosed = ComputeEnclosedByStructure();
            _enclosedCheckTick = kEnclosedRecheckTicks;
        }
    }

    {
        const bool sheltered = IsPlayerSheltered();
        if (_prevSheltered && !sheltered)
        {
            _cellExitGraceTicks = kCellExitGraceTicks;
            logger->info("[Radstorm] Shelter->exposed transition — radstorm grace armed ({} ticks).",
                _cellExitGraceTicks);
        }
        _prevSheltered = sheltered;
    }
    if (_cellExitGraceTicks > 0) --_cellExitGraceTicks;

    auto* sky = RE::Sky::GetSingleton();
    if (!sky || !sky->currentWeather)
    {
        _radstormWarned = false;
        return;
    }

    if (_debugWeatherHold) return;

    const uint32_t wid = sky->currentWeather->GetFormID();

    const bool isDust = _dustWeathers.contains(wid);
    const bool isRain = IsRainyWeather(sky->currentWeather);
    if (_stormPhase == StormPhase::Idle &&
        _playSeconds < kRadstormStartupGraceSec &&
        (isDust || isRain))
    {
        if (sky->currentWeatherPct < kRadstormVetoMaxPct)
        {
            RE::TESWeather* revertTo = sky->lastWeather;
            if (!revertTo || _dustWeathers.contains(revertTo->GetFormID()) ||
                IsRadstormWeather(revertTo->GetFormID()) || IsRainyWeather(revertTo))
                revertTo = _clearWeather;
            if (revertTo)
            {
                sky->ForceWeather(revertTo, false);
                logger->info("[Radstorm] Startup grace: {} 0x{:06X} vetoed at pct {:.2f} -> "
                             "0x{:06X} (play={:.0f}s).",
                    isDust ? "dust storm" : "rain", wid, sky->currentWeatherPct,
                    revertTo->GetFormID(), _playSeconds);
            }
        }
        return;
    }

    if (!IsRadstormWeather(wid))
    {
        if (_radstormWarned) StopStormCue(kRadstormCueFalloffMs);
        _radstormWarned = false;
        return;
    }
    if (_stormPhase != StormPhase::Idle) return;
    if (_radstormWarned) return;

    const bool inStartupGrace = _playSeconds < kRadstormStartupGraceSec;
    const bool inCooldown     = RadCorrectnessPolicy::InVanillaRadstormCooldown(_playSeconds, _lastRadstormSec,
                                                                          _vanillaRadstormGapSec);
    const bool inCellExitGrace = _cellExitGraceTicks > 0;
    if (_stormPhase == StormPhase::Idle && (inStartupGrace || inCooldown || inCellExitGrace))
    {
        RE::TESWeather* revertTo = sky->lastWeather;
        if (!revertTo || IsRadstormWeather(revertTo->GetFormID()))
            revertTo = _clearWeather;
        if (revertTo)
        {
            BeginSmoothWeather(revertTo, false);
            logger->info("[Radstorm] Vanilla radstorm 0x{:06X} vetoed at pct {:.2f} -> smoothly reverted to "
                         "0x{:06X} (reason: {}, play={:.0f}s).",
                wid, sky->currentWeatherPct, revertTo->GetFormID(),
                inCellExitGrace ? "cell-exit grace" : (inStartupGrace ? "startup grace" : "cooldown"),
                _playSeconds);
        }
        return;
    }

    _lastRadstormSec = _playSeconds;
    _radstormWarned  = true;

    logger->info("[Radstorm] Incoming vanilla radstorm (weather 0x{:06X}) — soft ambient cue.", wid);
    PlayStormCue();
    SpawnStormGhouls();
}


void RadiationEngine::BeginSmoothWeather(RE::TESWeather* a_target, bool a_lockOverride)
{
    auto* sky = RE::Sky::GetSingleton();
    if (!sky || !a_target) return;
    if (_releaseWeatherOverrideWhenSettled && a_target != _weatherReturnTarget)
    {
        _releaseWeatherOverrideWhenSettled = false;
        _weatherHandoffSettleTicks = 0;
        _weatherHandoffWaitTicks = 0;
    }
    if (sky->currentWeather == a_target)
    {
        if (a_lockOverride)
        {
            sky->overrideWeather = a_target;
            _weatherForced = true;
        }
        return;
    }
    if (a_lockOverride)
        sky->overrideWeather = a_target;
    sky->lastWeather       = sky->currentWeather;
    sky->currentWeather    = a_target;
    sky->lastWeatherUpdate  = sky->currentGameHour;
    sky->currentWeatherPct  = 0.0f;
    _weatherForced = true;
}

void RadiationEngine::CaptureWeatherReturnTarget()
{
    if (_weatherReturnCaptured) return;

    auto* sky = RE::Sky::GetSingleton();
    if (!sky || !sky->currentWeather)
    {
        if (_clearWeather)
        {
            _weatherReturnTarget = _clearWeather;
            _weatherReturnCaptured = true;
        }
        return;
    }

    RE::TESWeather* candidate = sky->currentWeather;
    bool isManagedStorm = false;
    for (auto* managed : _stormWeathers)
    {
        if (managed && managed == candidate)
        {
            isManagedStorm = true;
            break;
        }
    }

    const bool isVanillaRadstorm = IsRadstormWeather(candidate->GetFormID());
    _weatherReturnTarget = RadCorrectnessPolicy::ShouldReturnToClear(isManagedStorm, isVanillaRadstorm)
        ? _clearWeather
        : candidate;
    _weatherReturnCaptured = _weatherReturnTarget != nullptr;
    if (_weatherReturnCaptured)
        Log()->debug("[StormWeather] Captured pre-override return weather 0x{:06X}.", _weatherReturnTarget->GetFormID());
}


void RadiationEngine::UpdateWeatherHandoff()
{
    const bool resetSessionState = _forceClearOnEntry
        || (!_weatherForced && (_weatherReturnTarget || _weatherReturnCaptured
                                || _releaseWeatherOverrideWhenSettled || _weatherHandoffSettleTicks != 0
                                || _weatherHandoffWaitTicks != 0));
    if (resetSessionState)
    {
        _weatherReturnTarget = nullptr;
        _weatherReturnCaptured = false;
        _releaseWeatherOverrideWhenSettled = false;
        _weatherHandoffSettleTicks = 0;
        _weatherHandoffWaitTicks = 0;
        _weatherForced = false;
        return;
    }
    if (!_releaseWeatherOverrideWhenSettled) return;

    auto* sky = RE::Sky::GetSingleton();
    if (!sky)
    {
        _weatherReturnTarget = nullptr;
        _weatherReturnCaptured = false;
        _releaseWeatherOverrideWhenSettled = false;
        _weatherHandoffSettleTicks = 0;
        _weatherHandoffWaitTicks = 0;
        _weatherForced = false;
        return;
    }

    RE::TESWeather* target = _weatherReturnTarget;
    if (!target || sky->currentWeather != target)
    {
        sky->ReleaseWeatherOverride();
        _weatherReturnTarget = nullptr;
        _weatherReturnCaptured = false;
        _releaseWeatherOverrideWhenSettled = false;
        _weatherHandoffSettleTicks = 0;
        _weatherHandoffWaitTicks = 0;
        _weatherForced = false;
        Log()->warn("[StormWeather] Return target was lost or replaced; released the temporary weather override.");
        return;
    }

    if (!std::isfinite(sky->currentWeatherPct))
    {
        sky->ReleaseWeatherOverride();
        _weatherReturnTarget = nullptr;
        _weatherReturnCaptured = false;
        _releaseWeatherOverrideWhenSettled = false;
        _weatherHandoffSettleTicks = 0;
        _weatherHandoffWaitTicks = 0;
        _weatherForced = false;
        Log()->warn("[StormWeather] Return transition reported a non-finite blend; released the temporary weather override.");
        return;
    }

    if (++_weatherHandoffWaitTicks > 240)
    {
        sky->ReleaseWeatherOverride();
        _weatherReturnTarget = nullptr;
        _weatherReturnCaptured = false;
        _releaseWeatherOverrideWhenSettled = false;
        _weatherHandoffSettleTicks = 0;
        _weatherHandoffWaitTicks = 0;
        _weatherForced = false;
        Log()->warn("[StormWeather] Return transition exceeded its bounded wait; released the temporary weather override.");
        return;
    }

    if (sky->currentWeatherPct < 0.999f)
    {
        _weatherHandoffSettleTicks = 0;
        return;
    }

    if (++_weatherHandoffSettleTicks < 8) return;

    const std::uint32_t targetId = target->GetFormID();
    sky->ReleaseWeatherOverride();
    _weatherReturnTarget = nullptr;
    _weatherReturnCaptured = false;
    _releaseWeatherOverrideWhenSettled = false;
    _weatherHandoffSettleTicks = 0;
    _weatherHandoffWaitTicks = 0;
    _weatherForced = false;
    Log()->info("[StormWeather] Return weather 0x{:06X} stable; vanilla region weather control restored.", targetId);
}


void RadiationEngine::UpdateDustFogRoll()
{
    if (!_dustBuildupActive) return;
    auto* sky = RE::Sky::GetSingleton();
    RE::TESWeather* dust = _stormWeathers[kDustStormIndex];
    if (!sky || !dust || sky->currentWeather != dust)
    {
        _dustBuildupActive = false;
        _dustFogRollTick   = 0;
        return;
    }

    const bool arrived = (sky->lastWeather == nullptr) || (sky->currentWeatherPct >= 0.98f);
    if (_dustRollElapsed++ % 8 == 0)
        Log()->info("[RollDiag] pct={:.2f} lastWeather={} (engine-native transition)",
            sky->currentWeatherPct, sky->lastWeather ? "set" : "cleared");
    if (arrived)
    {
        _dustBuildupActive = false;
        _dustFogRollTick   = 0;
        Log()->info("[StormFog] Dust Storm transition complete (engine-native, no snap).");

        {
            auto lg = Log();
            float hour = -1.0f;
            if (auto* cal = RE::Calendar::GetSingleton(); cal && cal->gameHour)
                hour = cal->gameHour->GetValue();
            RE::TESImageSpace* isNight = dust->imageSpace[3];
            RE::TESImageSpace* isDay   = dust->imageSpace[1];
            lg->info("[PurpleDiag] LIVE dust weather @ hour {:.1f}: imageSpace Day(1)=0x{:06X} Night(3)=0x{:06X} "
                     "(CommonwealthDusty=0x1F61A5/0x1F61A9, old radstorm=0x224586/0x22458C).",
                hour, isDay ? isDay->GetFormID() : 0, isNight ? isNight->GetFormID() : 0);
            bool allDusty = true;
            for (int k = 0; k < 8; ++k)
            {
                RE::TESImageSpace* is = dust->imageSpace[k];
                const std::uint32_t id = is ? is->GetFormID() : 0;
                if (id < 0x1F61A2 || id > 0x1F61A9) allDusty = false;
            }
            lg->info("[PurpleDiag] all 8 imagespace slots CommonwealthDusty (brown) = {}.", allDusty ? "YES" : "NO");
        }

        if (_dustBurstArt && !IsPlayerSheltered())
        {
            if (auto* player = RE::PlayerCharacter::GetSingleton())
            {
                player->ApplyArtObject(_dustBurstArt, kDustBurstSec, nullptr, true,
                    true);
                Log()->info("[StormFog] Dust wall-arrival heavy-dust burst fired ({:.0f}s).", kDustBurstSec);
            }
        }
    }
}
