#include "RadHud.h"
#include "PrismaUI_F4_API.h"

#include <chrono>
#include <string_view>
#include <thread>

namespace
{
    void SuppressVanillaMetersOnPrismaThread(void* userData)
    {
        auto* api11 = static_cast<PRISMA_UI_API::IVPrismaUI11*>(userData);
        if (!api11 || !RadHud::GetSingleton().ViewsReady()) return;
        if (RadHud::GetSingleton().IsCompanionMode()) return;
        api11->SuppressHUDWidget("HUDPlayerHealthMeter", true);
        api11->SuppressHUDWidget("HUDActionPointMeter", true);
    }

    void CreateRadHudViewOnPrismaThread(void* userData)
    {
        auto& hud = RadHud::GetSingleton();
        auto* api11 = static_cast<PRISMA_UI_API::IVPrismaUI11*>(userData);
        if (!hud.LoadSettled()) return;
        if (hud.ViewsExist())
        {
            SuppressVanillaMetersOnPrismaThread(api11);
            return;
        }
        if (api11 && !hud.IsCompanionMode())
        {
            api11->SuppressHUDWidget("HUDPlayerHealthMeter", false);
            api11->SuppressHUDWidget("HUDActionPointMeter", false);
        }
        hud.CreateView();
        if (hud.ViewsReady())
            SuppressVanillaMetersOnPrismaThread(api11);
    }
}

bool RadHud::StartPrismaViewRetry()
{
    if (!_threadApi)
        _threadApi = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>();
    if (!_threadApi) return false;

    const auto nowMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if (nowMs < _viewRetryNotBeforeMs.load()) return _viewCreateRetry.load() || ViewsExist();
    if (_viewCreateRetry.exchange(true)) return true;
    _viewRetryNotBeforeMs.store(nowMs + 1000);

    auto* api11 = static_cast<PRISMA_UI_API::IVPrismaUI11*>(_threadApi);
    if (ViewsExist())
    {
        _viewCreateRetry.store(false);
        api11->DispatchToGameThread(SuppressVanillaMetersOnPrismaThread, api11);
        return true;
    }
    auto* retrying = &_viewCreateRetry;
    std::thread([api11, retrying]() {
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            if (api11->DispatchToGameThread(CreateRadHudViewOnPrismaThread, api11))
            {
                for (int settle = 0; settle < 10; ++settle)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    if (RadHud::GetSingleton().ViewsReady() &&
                        api11->DispatchToGameThread(SuppressVanillaMetersOnPrismaThread, api11))
                    {
                        retrying->store(false);
                        return;
                    }
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        retrying->store(false);
    }).detach();
    return true;
}

namespace
{
    void NotePhosphorView(PrismaView, const char* htmlPath, void* userdata)
    {
        if (htmlPath && std::string_view(htmlPath).find("PhosphorHUD") != std::string_view::npos)
            *static_cast<bool*>(userdata) = true;
    }
}

void RadHud::BeginLoadSettle() noexcept
{
    _ticksSinceLoad.store(0);
    _phosphorViewsSeen.store(false);
}

bool RadHud::LoadSettled() const noexcept
{
    return RadCorrectnessPolicy::LoadSettleElapsed(
        _ticksSinceLoad.load() * RadCorrectnessPolicy::kEngineTickMs, 0, RadCorrectnessPolicy::kLoadSettleMs);
}

void RadHud::TickLoadSettle()
{
    _ticksSinceLoad.fetch_add(1);
    if (!_phosphorViewsSeen.load() && LoadSettled() && IsCompanionMode() &&
        _ticksSinceLoad.load() <= RadCorrectnessPolicy::kCompanionBridgeMaxTicks)
    {
        if (ScanPhosphorViews())
            _phosphorViewsSeen.store(true);
    }
}

bool RadHud::BridgeReady()
{
    return LoadSettled() && RadCorrectnessPolicy::CompanionBridgeReady(
        IsCompanionMode(), _phosphorViewsSeen.load(), _ticksSinceLoad.load());
}

bool RadHud::PrismaWindowOpen()
{
    return BridgeReady();
}

void RadHud::NoteDomReady(int slot, uint64_t view) noexcept
{
    if (slot < 0 || slot >= kDomSlotCount) return;
    const auto live = _domLive[slot].load(std::memory_order_acquire);
    if (live != 0 && live != view) return;
    _domPending[slot].store(view, std::memory_order_release);
}

bool RadHud::ScanPhosphorViews()
{
    if (!_enumApi)
        _enumApi = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
    if (!_enumApi) return false;
    bool found = false;
    static_cast<PRISMA_UI_API::IVPrismaUI4*>(_enumApi)->EnumerateViews(NotePhosphorView, &found);
    return found;
}

void RadHud::PumpDomReady()
{
    UpdateMenuCover();
    if (!_api || !PrismaWindowOpen()) return;
    const auto pendingMain = _domPending[kDomSlotMain].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingMain, _view)) FinalizeMainDomReady(pendingMain);
    const auto pendingToxic = _domPending[kDomSlotToxic].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingToxic, _toxicView)) FinalizeToxicDomReady(pendingToxic);
    const auto pendingFilter = _domPending[kDomSlotFilter].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingFilter, _filterView)) FinalizeFilterDomReady(pendingFilter);
    const auto pendingClimate = _domPending[kDomSlotClimate].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingClimate, _climateView)) FinalizeClimateDomReady(pendingClimate);
    const auto pendingNeeds = _domPending[kDomSlotNeeds].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingNeeds, _needsView)) FinalizeNeedsDomReady(pendingNeeds);
    const auto pendingOutbreak = _domPending[kDomSlotOutbreak].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingOutbreak, _outbreakView)) FinalizeOutbreakDomReady(pendingOutbreak);
    const auto pendingCompanions = _domPending[kDomSlotCompanions].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingCompanions, _companionsView)) FinalizeCompanionsDomReady(pendingCompanions);
    const auto pendingDebug = _domPending[kDomSlotDebug].exchange(0, std::memory_order_acq_rel);
    if (RadCorrectnessPolicy::IsLiveDomReady(pendingDebug, _debugView)) FinalizeDebugDomReady(pendingDebug);
    if (_widgetReapplyPending.exchange(false)) ReapplyWidgetRegistrations();
}
