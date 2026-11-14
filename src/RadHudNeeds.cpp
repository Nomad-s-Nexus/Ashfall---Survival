#include "pch.h"
#include "RadHud.h"
#include "LimbInjuryRuntime.h"
#include "RadNeedsTuning.h"
#include <RE/D/DifficultyLevel.h>
#include <RE/P/PlayerCharacter.h>
#include "PrismaUI_F4_API.h"
#include "PrismaHUD_F4_API.h"

namespace
{
    bool SurvivalLayerActive()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return !player || player->GetDifficultyLevel() != RE::DifficultyLevel::kTrueSurvival;
    }
}

void RadHud::FinalizeNeedsDomReady(uint64_t view)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(view)) return;
    PushTheme(view);
    _needsReadyView.store(view);
}

void RadHud::PushNeeds(const RadNeedsPolicy::NeedsSnapshot& snapshot)
{
    _youDisease = snapshot.diseasePacked;
    _youOnset = static_cast<int>(std::lround(RadNeedsPolicy::Clamp01(snapshot.diseaseOnset01) * 100.0f));
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!_needsView || !api->IsValid(_needsView))
    {
        _needsReadyView.store(0);
        _needsWidgetRegistered = false;
        _needsPayload.clear();
        if (!PlayerInWorld() || !LoadSettled()) return;
        StartPrismaViewRetry();
        _domLive[kDomSlotNeeds].store(0);
        _needsView = api->CreateView("Interface/FalloutAnomaly/hud_needs.html", [](PrismaView view) {
            RadHud::NoteDomReady(RadHud::kDomSlotNeeds, view);
        });
        _domLive[kDomSlotNeeds].store(_needsView);
        if (_needsView && api->IsValid(_needsView))
        {
            api->SetOrder(_needsView, 1'000'000);
            api->Hide(_needsView);
            _coverHidden[kDomSlotNeeds] = 0;
        }
    }
    if (!_needsView || !api->IsValid(_needsView) || _needsReadyView.load() != _needsView) return;
    if (IsCompanionMode() && !SurvivalLayerActive())
    {
        if (!_needsPayload.empty())
        {
            api->Hide(_needsView);
            _coverHidden[kDomSlotNeeds] = 0;
            _needsPayload.clear();
        }
        return;
    }
    if (_hudApi && !_needsWidgetRegistered && BridgeReady())
    {
        auto* hud = static_cast<PRISMA_HUD_API::IVPrismaHUD1*>(_hudApi);
        hud->RegisterWidget(_needsView, CompanionPlaced({ "rad_needs", "Needs", 22.0f / 1920.0f,
            746.0f / 1080.0f, 360.0f / 1920.0f, 98.0f / 1080.0f }));
        hud->ApplyLayout(_needsView);
        _needsWidgetRegistered = true;
    }
    SyncLocalHudLayout();
    const auto percent = [](float value) {
        return static_cast<int>(std::lround(RadNeedsPolicy::Clamp01(value) * 100.0f));
    };
    const auto limbs = LimbInjuryRuntime::GetSingleton().GetHudSnapshot();
    int        limb[LimbInjuryPolicy::kLimbCount]{};
    for (std::size_t i = 0; i < LimbInjuryPolicy::kLimbCount; ++i)
        limb[i] = static_cast<int>(LimbInjuryPolicy::Encode(limbs.states[i]));
    char payload[320];
    std::snprintf(payload, sizeof(payload),
        R"({"hunger":%d,"thirst":%d,"sleepDebt":%d,"risk":%d,"disease":%d,"onset":%d,"always":%s,"limbs":[%d,%d,%d,%d,%d,%d]})",
        percent(snapshot.pools.hunger01), percent(snapshot.pools.thirst01),
        percent(snapshot.pools.sleepDebt01), percent(snapshot.risk01), snapshot.diseasePacked,
        percent(snapshot.diseaseOnset01),
        RadNeedsTuning::On(RadNeedsTuningPolicy::kAlwaysShowNeeds, false) ? "true" : "false",
        limb[0], limb[1], limb[2], limb[3], limb[4], limb[5]);
    if (_needsPayload == payload) return;
    _needsPayload = payload;
    api->InteropCall(_needsView, "updateNeeds", payload);
    api->Show(_needsView);
}
void RadHud::PushSurvival(const RadNeedsPolicy::NeedsSnapshot& snapshot)
{
    if (!_api || !_debugOpen || _debugView == 0 || !_debugDomReady) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView)) return;
    const auto percent = [](float value) {
        return static_cast<int>(std::lround(RadNeedsPolicy::Clamp01(value) * 100.0f));
    };
    const auto limbs = LimbInjuryRuntime::GetSingleton().GetHudSnapshot();
    auto       limbJson = json::array();
    for (std::size_t i = 0; i < LimbInjuryPolicy::kLimbCount; ++i)
    {
        const auto& s = limbs.states[i];
        limbJson.push_back({
            { "t", s.trauma }, { "b", s.bleed }, { "s", s.splinted ? 1 : 0 }, { "q", s.tourniquet ? 1 : 0 },
            { "h", std::round(s.heal * 100.0f) / 100.0f }, { "c", std::round(limbs.condition[i] * 10.0f) / 10.0f } });
    }
    const auto payload = json{
        { "hunger", percent(snapshot.pools.hunger01) },
        { "thirst", percent(snapshot.pools.thirst01) },
        { "sleepDebt", percent(snapshot.pools.sleepDebt01) },
        { "risk", percent(snapshot.risk01) },
        { "disease", snapshot.diseasePacked },
        { "limbs", limbJson },
        { "limbFx", json::array({ limbs.fx.legs, limbs.fx.arms, limbs.fx.head, limbs.fx.torso }) }
    }.dump();
    if (payload == _survivalPayload) return;
    _survivalPayload = payload;
    api->InteropCall(_debugView, "updateSurvival", payload.c_str());
}
