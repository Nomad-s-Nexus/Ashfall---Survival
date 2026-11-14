#include "pch.h"
#include "RadHud.h"

#include "PrismaUI_F4_API.h"

namespace
{
    constexpr const char* kOutbreakViewPath = "Interface/FalloutAnomaly/hud_outbreak.html";
    constexpr int kHudViewOrder = 1'000'000;
}

void RadHud::FinalizeOutbreakDomReady(uint64_t view)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(view)) return;
    PushTheme(view);
    _outbreakReadyView.store(view);
}

void RadHud::PushOutbreak(const std::string& payload)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    const bool empty = payload == "[]";
    if (!_outbreakView || !api->IsValid(_outbreakView))
    {
        _outbreakReadyView.store(0);
        _outbreakPayload.clear();
        if (empty || !PlayerInWorld() || !LoadSettled()) return;
        _domLive[kDomSlotOutbreak].store(0);
        _outbreakView = api->CreateView(kOutbreakViewPath, [](PrismaView view) {
            RadHud::NoteDomReady(RadHud::kDomSlotOutbreak, view);
        });
        _domLive[kDomSlotOutbreak].store(_outbreakView);
        if (_outbreakView && api->IsValid(_outbreakView))
        {
            api->SetOrder(_outbreakView, kHudViewOrder);
            api->Hide(_outbreakView);
            _coverHidden[kDomSlotOutbreak] = 0;
        }
    }
    if (!_outbreakView || !api->IsValid(_outbreakView) || _outbreakReadyView.load() != _outbreakView) return;
    if (payload == _outbreakPayload) return;
    _outbreakPayload = payload;
    api->InteropCall(_outbreakView, "updateOutbreak", payload.c_str());
    if (empty)
    {
        api->Hide(_outbreakView);
        _coverHidden[kDomSlotOutbreak] = 0;
    }
    else
    {
        api->Show(_outbreakView);
    }
}

void RadHud::HideOutbreak()
{
    _outbreakPayload.clear();
    _coverHidden[kDomSlotOutbreak] = 0;
    if (_api && _outbreakView)
    {
        auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
        if (api->IsValid(_outbreakView)) api->Hide(_outbreakView);
    }
}
