#include "RadEngine_Internal.h"
#include "RadCorrectnessPolicy.h"
#include "RadNeedsTuning.h"
#include <RE/P/PlayerCamera.h>
#include <RE/N/NiControllerManager.h>
#include <RE/N/NiControllerSequence.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiObjectNET.h>

using namespace RadInternal;

void RadiationEngine::EffectiveStormGap(float& a_min, float& a_max) const
{
    a_min = std::max(1.0f, RadNeedsTuning::Value(RadNeedsTuningPolicy::kStormMinMinutes, _stormMinRealMin));
    a_max = std::max(a_min, RadNeedsTuning::Value(RadNeedsTuningPolicy::kStormMaxMinutes, _stormMaxRealMin));
}


void RadiationEngine::ScheduleNextStorm()
{
    static std::mt19937 rng{ std::random_device{}() };
    float lo = 0.0f, hi = 0.0f;
    EffectiveStormGap(lo, hi);
    std::uniform_real_distribution<float> mins(lo, hi);
    const float realMin = mins(rng);

    _nextStormPlaySec = _playSeconds + static_cast<double>(realMin) * 60.0;
    _stormScheduled   = true;
    Log()->info(
        "[Storm] Next storm in ~{:.0f} real min of play (at {:.0f}s, now {:.0f}s).",
        realMin, _nextStormPlaySec, _playSeconds);
}
void RadiationEngine::BeginStorm(int typeIndex)
{
    if (typeIndex < 0 || typeIndex >= kStormTypeTableSize) return;
    auto logger = Log();

    _activeStormType   = typeIndex;
    _stormExposedTicks = 0;
    _fleeTick          = 0;
    _stormFXVisible    = false;
    _stormNukeFired    = false;
    _firestormAwaitingNuke = false;
    _stormRollInTicks  = 0;
    _nukeAttempts      = 0;
    _nukeWaitTicks     = 0;
    _shakeBurstRetries = 0;
    _debugSkipToActive = false;
    const StormType& st = kStormTypes[typeIndex];

    const bool debugForced = _forcedStormType >= 0;
    const int leadTicks = (debugForced && _forcedLeadTicks >= 0) ? _forcedLeadTicks : _stormLeadTicks;
    _forcedLeadTicks = -1;
    _activeLeadTicks = leadTicks;
    logger->warn("========== STORM APPROACHING: {} ({}) — silent lead {} s ==========",
        st.name, st.lethal ? "LETHAL" : "ambient", leadTicks / 4);
    _sirenDelayTicks = 0;
    _dustBuildupActive = false;
    _debugWeatherHold  = false;

    EnterStormPhase(StormPhase::Approach, leadTicks);
    logger->info("[StormSeq] t=0 APPROACH begin: silent. Alarm in {} s.", leadTicks / 4);
}
void RadiationEngine::EnterWarningPhase()
{
    if (_activeStormType < 0 || _activeStormType >= kStormTypeTableSize) return;
    auto logger = Log();
    const StormType& st = kStormTypes[_activeStormType];
    const int warnTicks = st.warningTicks;
    EnterStormPhase(StormPhase::Warning, warnTicks);

    if (!IsPlayerSheltered())
    {
        PlayStormCue();
    }
    UpdateStormFXVisibility();
    char msg[96];
    if (st.lethal)
        std::snprintf(msg, sizeof(msg), "WARNING: %s approaching! SEEK SHELTER NOW!", st.name);
    else
        std::snprintf(msg, sizeof(msg), "WARNING: %s is rolling in.", st.name);
    Notify(msg, true);
    logger->info("[StormSeq] WARNING begin: alarm + alert fired.");

    const auto warningSky = RadCorrectnessPolicy::StormSkyChoice(_activeStormType, RadCorrectnessPolicy::kPhaseWarning);
    if (warningSky != RadCorrectnessPolicy::StormSkyKind::None)
    {
        if (auto* sky = RE::Sky::GetSingleton())
        {
            RE::TESWeather* dust = StormSkyWeather(_activeStormType, warningSky);
            RE::TESWeather* prev = sky->currentWeather;
            if (dust && prev && prev != dust)
            {
                CaptureWeatherReturnTarget();
                BeginSmoothWeather(dust);
                _dustBuildupActive = true;
                _warnTicksTotal    = warnTicks;
                _dustRollElapsed   = 0;
                _dustRollTotal     = warnTicks;
                logger->info("[StormSeq] dust-sky roll-in armed (from 0x{:06X}).", prev->GetFormID());
            }
        }
    }
}
int RadiationEngine::RollStormType() const
{
    int total = 0;
    for (const auto& s : kStormTypes) total += s.weight;
    if (total <= 0) return 0;

    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> pick(0, total - 1);
    int r = pick(rng);
    for (int i = 0; i < kStormTypeTableSize; ++i)
    {
        r -= kStormTypes[i].weight;
        if (r < 0) return i;
    }
    return 0;
}
void RadiationEngine::EnterStormPhase(StormPhase phase, int durationTicks)
{
    _stormPhase = phase;

    if (_debugSkipToActive && (phase == StormPhase::Approach || phase == StormPhase::Warning))
        durationTicks = 1;
    if (phase == StormPhase::Active)
        _debugSkipToActive = false;

    _stormTicksLeft = durationTicks;

    if (phase != StormPhase::Active)
    {
        _shakeBurstStep    = -1;
        _shakeRumbleTick   = -1;
        _shakeBurstRetries = 0;
        _firestormAwaitingNuke = false;
        _nukeAttempts      = 0;
        _nukeWaitTicks     = 0;
    }
}
