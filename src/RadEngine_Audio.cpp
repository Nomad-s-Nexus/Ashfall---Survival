#include "RadEngine_Internal.h"
#include "RadCorrectnessPolicy.h"

using namespace RadInternal;

void RadiationEngine::PlayStormCue()
{
    auto* audio = RE::BSAudioManager::GetSingleton();
    if (!audio) { Log()->warn("[StormCue] BSAudioManager unavailable; cannot play siren."); return; }
    if (_radstormCueHandle.IsValid()) _radstormCueHandle.Stop();
    RE::BSResource::ID file(kStormSoundPath);
    audio->GetSoundHandleByFile(_radstormCueHandle, file, 0x10, 0);
    if (_radstormCueHandle.IsValid())
    {
        _radstormCueHandle.SetStaticAttenuation(kRadstormCueAttenuation);
        _radstormCueHandle.FadeInPlay(kRadstormCueFadeMs);
        Log()->info("[StormCue] Siren started ('{}').", kStormSoundPath);
    }
    else
    {
        Log()->warn("[StormCue] Sound handle invalid for '{}' (check file exists / format).", kStormSoundPath);
    }
}


void RadiationEngine::StopStormCue(std::uint16_t fadeMs)
{
    if (_radstormCueHandle.IsValid()) _radstormCueHandle.FadeOutAndRelease(fadeMs);
}


void RadiationEngine::EnsureBed(RE::BSSoundHandle& handle, const char*& curPath,
                                const char* wantPath, std::uint16_t atten, std::uint16_t& applied)
{
    if (!wantPath)
    {
        if (handle.IsValid()) { handle.FadeOutAndRelease(1000); handle = RE::BSSoundHandle{}; }
        curPath = nullptr;
        applied = 0xFFFF;
        return;
    }
    if (curPath != wantPath)
    {
        if (handle.IsValid()) { handle.FadeOutAndRelease(500); handle = RE::BSSoundHandle{}; }
        curPath = wantPath;
    }
    if (!handle.IsValid() || !handle.IsPlaying())
    {
        if (Play2DSound(wantPath, handle))
        {
            handle.SetStaticAttenuation(atten);
            applied = atten;
        }
    }
    else if (applied != atten)
    {
        handle.SetStaticAttenuation(atten);
        applied = atten;
    }
}


void RadiationEngine::UpdateAmbientAudio()
{
    auto* sky = RE::Sky::GetSingleton();
    const std::uint32_t wid = (sky && sky->currentWeather) ? sky->currentWeather->GetFormID() : 0;

    const char* stormBed = nullptr;
    if (wid)
        for (int i = 0; i < kStormTypeTableSize; ++i)
            if (kStormTypes[i].weatherID == wid) { stormBed = kStormTypes[i].ambient; break; }

    if (stormBed && _dustBuildupActive && _stormPhase == StormPhase::Warning)
        stormBed = nullptr;
    if (!stormBed && _activeStormType >= 0 && _activeStormType < kStormTypeTableSize &&
        RadCorrectnessPolicy::VanillaSkyStormBedAllowed(_activeStormType, static_cast<int>(_stormPhase)))
    {
        const RE::TESWeather* shared = StormSkyWeather(_activeStormType,
            RadCorrectnessPolicy::StormSkyChoice(_activeStormType, static_cast<int>(_stormPhase)));
        if (shared && shared->GetFormID() == wid)
            stormBed = kStormTypes[_activeStormType].ambient;
    }

    const bool interior = [] {
        auto* pc = RE::PlayerCharacter::GetSingleton();
        auto* cell = pc ? pc->GetParentCell() : nullptr;
        return cell && cell->IsInterior();
    }();
    const bool exposed = !IsPlayerSheltered();

    if (stormBed && exposed)
    {
        EnsureBed(_stormSoundHandle, _curStormBed, stormBed, _stormBedAtten, _stormBedApplied);
        EnsureBed(_ambientBedHandle, _curAmbientBed, nullptr, 0, _ambientBedApplied);
    }
    else
    {
        EnsureBed(_stormSoundHandle, _curStormBed, nullptr, 0, _stormBedApplied);
        const char* want = (_ambientEnabled && !interior) ? kAmbientBedPath : nullptr;
        EnsureBed(_ambientBedHandle, _curAmbientBed, want, _ambientAtten, _ambientBedApplied);
    }
}
