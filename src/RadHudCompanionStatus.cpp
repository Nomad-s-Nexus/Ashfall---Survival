#include "pch.h"
#include "RadHud.h"
#include "RadNeedsTuning.h"

#include "PrismaHUD_F4_API.h"
#include "PrismaUI_F4_API.h"

namespace
{
    constexpr const char* kCompanionsViewPath = "Interface/FalloutAnomaly/hud_companions.html";
    constexpr int kHudViewOrder = 1'000'000;
}

void RadHud::FinalizeCompanionsDomReady(uint64_t view)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(view)) return;
    PushTheme(view);
    _companionsReadyView.store(view);
}

void RadHud::PushCompanions(const std::string& list)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    const bool always = RadNeedsTuning::On(RadNeedsTuningPolicy::kAlwaysShowNeeds, false);
    const bool empty = list == "[]" && _youDisease == 0 && !always;
    if (!_companionsView || !api->IsValid(_companionsView))
    {
        _companionsReadyView.store(0);
        _companionsWidgetRegistered = false;
        _companionsPayload.clear();
        if (empty || !PlayerInWorld() || !LoadSettled()) return;
        _domLive[kDomSlotCompanions].store(0);
        _companionsView = api->CreateView(kCompanionsViewPath, [](PrismaView view) {
            RadHud::NoteDomReady(RadHud::kDomSlotCompanions, view);
        });
        _domLive[kDomSlotCompanions].store(_companionsView);
        if (_companionsView && api->IsValid(_companionsView))
        {
            api->SetOrder(_companionsView, kHudViewOrder);
            api->Hide(_companionsView);
            _coverHidden[kDomSlotCompanions] = 0;
        }
    }
    if (!_companionsView || !api->IsValid(_companionsView) || _companionsReadyView.load() != _companionsView) return;
    if (_hudApi && !_companionsWidgetRegistered && BridgeReady())
    {
        auto* hud = static_cast<PRISMA_HUD_API::IVPrismaHUD1*>(_hudApi);
        hud->RegisterWidget(_companionsView, CompanionPlaced({ "rad_companions", "Companions: sickness",
            22.0f / 1920.0f, 530.0f / 1080.0f, 360.0f / 1920.0f, 98.0f / 1080.0f }));
        hud->ApplyLayout(_companionsView);
        _companionsWidgetRegistered = true;
    }
    SyncLocalHudLayout();
    const auto payload = std::string(R"({"always":)") + (always ? "true" : "false") +
        R"(,"you":{"d":)" + std::to_string(_youDisease) + R"(,"o":)" + std::to_string(_youOnset) +
        R"(},"list":)" + list + "}";
    if (_companionsPayload == payload) return;
    _companionsPayload = payload;
    api->InteropCall(_companionsView, "updateCompanions", payload.c_str());
    api->Show(_companionsView);
}

void RadHud::HideCompanions()
{
    _companionsPayload.clear();
    _coverHidden[kDomSlotCompanions] = 0;
    if (_api && _companionsView)
    {
        auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
        if (api->IsValid(_companionsView)) api->Hide(_companionsView);
    }
}

void RadHud::PushOutbreakDebug(const std::string& payload)
{
    if (!_api || !_debugOpen || _debugView == 0 || !_debugDomReady) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView)) return;
    if (payload == _outbreakDebugPayload) return;
    _outbreakDebugPayload = payload;
    api->InteropCall(_debugView, "updateOutbreakStatus", payload.c_str());
}
