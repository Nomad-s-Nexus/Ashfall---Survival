#include "pch.h"
#include "RadHud.h"
#include "RadiationEngine.h"
#include "Hooks.h"
#include "ClimateRuntime.h"
#include "RadNeedsTuning.h"

#include <RE/A/ActorValue.h>
#include <RE/A/ACTOR_VALUE_MODIFIER.h>
#include <RE/B/BSTEvent.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/M/MessageBoxMenu.h>
#include <RE/P/PauseMenu.h>
#include <RE/U/UI.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/PowerArmor.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/B/BGSInventoryList.h>
#include <RE/B/BGSBipedObjectForm.h>
#include <RE/B/BIPED_OBJECT.h>
#include <RE/C/Calendar.h>
#include <RE/T/TESGlobal.h>
#include <RE/S/SendHUDMessage.h>

#include "PrismaUI_F4_API.h"
#include "PrismaHUD_F4_API.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include <cstdio>

namespace
{
    constexpr const char* kViewPath = "Interface/FalloutAnomaly/radhud.html";
    constexpr const char* kDebugViewPath = "Interface/FalloutAnomaly/rad_debug.html";
    constexpr const char* kToxicViewPath = "Interface/FalloutAnomaly/hud_toxic.html";
    constexpr const char* kToxicWidgetId = "rad_toxic";
    constexpr const char* kFilterViewPath = "Interface/FalloutAnomaly/hud_filter.html";
    constexpr const char* kFilterWidgetId = "rad_filter";
    constexpr int kHudViewOrder = 1'000'000;

    constexpr PRISMA_HUD_API::WidgetDesc kToxicDesc{
        kToxicWidgetId, "Toxicity", 22.0f / 1920.0f, 638.0f / 1080.0f, 360.0f / 1920.0f, 98.0f / 1080.0f };
    constexpr PRISMA_HUD_API::WidgetDesc kFilterDesc{
        kFilterWidgetId, "Gas Mask Filter", 22.0f / 1920.0f, 962.0f / 1080.0f, 360.0f / 1920.0f, 98.0f / 1080.0f };

    std::atomic<bool> g_inWorld{ false };

    bool IsPlayerInWorld()
    {
        if (g_inWorld.load()) return true;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->GetCurrentLocation()) return false;

        auto* inv = player->inventoryList;
        if (!inv) return false;

        bool hasPipboy = false;
        inv->ForEachStack(
            [](RE::BGSInventoryItem& a_item) -> bool {
                auto* obj = a_item.object;
                if (!obj || obj->GetFormType() != RE::ENUM_FORM_ID::kARMO) return false;
                auto* armo = static_cast<RE::TESObjectARMO*>(obj);
                return armo->FillsBipedSlot(static_cast<std::int32_t>(RE::BIPED_OBJECT::kPipboy));
            },
            [&hasPipboy](RE::BGSInventoryItem&, RE::BGSInventoryItem::Stack&) -> bool {
                hasPipboy = true;
                return false;
            });

        if (hasPipboy) { g_inWorld.store(true); return true; }

        if (auto* cal = RE::Calendar::GetSingleton(); cal && cal->gameYear &&
            cal->gameYear->GetValue() > 2100.0f)
        {
            g_inWorld.store(true);
            return true;
        }
        return false;
    }

    float EffectiveActorValueMax(const RE::PlayerCharacter& player, const RE::ActorValueInfo& value)
    {
        return std::max(0.0f,
            player.GetPermanentActorValue(value) +
            player.GetModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, value));
    }

    void OnRadDebugCommand(const char* arg)
    {
        if (arg && std::string_view(arg) == "close")
        {
            RadHud::GetSingleton().RequestDebugClose();
            return;
        }
        RadiationEngine::GetSingleton().HandleDebugCommand(arg ? arg : "");
    }

    void OnRadHudLayoutCommand(const char* arg)
    {
        std::string command = "hud:layout:";
        command += arg ? arg : "";
        RadiationEngine::GetSingleton().HandleDebugCommand(command.c_str());
    }

    class BlockingMenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        static BlockingMenuSink* GetSingleton() { static BlockingMenuSink s; return &s; }
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (a_event.opening &&
                (a_event.menuName == RE::PauseMenu::MENU_NAME ||
                 a_event.menuName == RE::MessageBoxMenu::MENU_NAME))
                RadHud::GetSingleton().OnBlockingMenuOpened();
            RadHud::GetSingleton().UpdateMenuCover(a_event.menuName.c_str(), a_event.opening);
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void RadHud::RequestAPI()
{
    auto logger = spdlog::get("FalloutAnomaly");

    if (!_api)
        _api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
    if (!_api)
    {
        _hudApi = nullptr;
        logger->info("[HUD] PrismaUI_F4 not present — radiation HUD disabled (optional).");
        return;
    }
    logger->info("[HUD] PrismaUI_F4 API acquired — radiation HUD enabled.");

    if (!_hudApi)
        _hudApi = PRISMA_HUD_API::RequestPluginAPI();
    if (!_hudEditorApi)
        _hudEditorApi = PRISMA_HUD_API::RequestEditorAPI();
    logger->info("[HUD] PrismaHUD_F4 {} — {}.",
        _hudApi ? "API acquired" : "not present",
        _hudApi ? "toxicity widget will be positionable in the HUD editor"
                : "standalone health, stamina, and tissue HUD enabled");

    if (PlayerInWorld())
        StartPrismaViewRetry();
}

bool RadHud::PlayerInWorld() const
{
    return IsPlayerInWorld();
}

bool RadHud::ViewsExist() const
{
    if (!_api) return false;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    return _view != 0 && api->IsValid(_view) &&
           _toxicView != 0 && api->IsValid(_toxicView) &&
           _filterView != 0 && api->IsValid(_filterView);
}

void RadHud::CreateView()
{
    auto logger = spdlog::get("FalloutAnomaly");
    if (!_api || !LoadSettled()) return;
    _viewsReady.store(false);

    g_inWorld.store(false);
    _maskPushed = -1;
    _toxicPayload.clear();
    _climatePayload.clear();
    _filterMainPayload.clear();
    _filterBarPayload.clear();
    _stormPayload.clear();
    _keysReplaceSent = -2;
    _keysRepairSent = -2;

    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);

    const auto setHudViewOrder = [api](PrismaView view) {
        if (view != 0 && api->IsValid(view))
            api->SetOrder(view, kHudViewOrder);
    };
    setHudViewOrder(_toxicView);
    setHudViewOrder(_filterView);

    static bool s_menuSinkRegistered = false;
    if (!s_menuSinkRegistered)
    {
        if (auto* ui = RE::UI::GetSingleton())
        {
            ui->RegisterSink(BlockingMenuSink::GetSingleton());
            s_menuSinkRegistered = true;
            logger->info("[HUD] Blocking-menu sink registered (pause menu + message boxes close the debug menu).");
        }
    }

    _toxicWidgetRegistered.store(_hudApi != nullptr);

    if (_view == 0 || !api->IsValid(_view))
    {
        _domReady = false;

        _domLive[kDomSlotMain].store(0);
        _view = api->CreateView(kViewPath, [](PrismaView v) { RadHud::NoteDomReady(kDomSlotMain, v); });
        _domLive[kDomSlotMain].store(_view);

        if (_view != 0) { logger->info("[HUD] Radiation overlay view created (handle {}).", _view); if (!_domReady.load()) { api->Hide(_view); _coverHidden[kDomSlotMain] = 0; } }
        else            logger->warn("[HUD] CreateView returned invalid handle.");
    }

    if (_toxicView == 0 || !api->IsValid(_toxicView))
    {
        _toxicDomReady = false;

        _domLive[kDomSlotToxic].store(0);
        _toxicView = api->CreateView(kToxicViewPath, [](PrismaView v) { RadHud::NoteDomReady(kDomSlotToxic, v); });
        _domLive[kDomSlotToxic].store(_toxicView);

        if (_toxicView != 0) { setHudViewOrder(_toxicView); logger->info("[HUD] Toxicity widget view created (handle {}).", _toxicView); if (!_toxicDomReady.load()) { api->Hide(_toxicView); _coverHidden[kDomSlotToxic] = 0; } }
    }
    else if (_hudApi && api->IsValid(_toxicView))
    {
        _widgetReapplyPending.store(true);
    }

    if (_filterView == 0 || !api->IsValid(_filterView))
    {
        _airPayload.clear();
        _filterDomReady = false;

        _domLive[kDomSlotFilter].store(0);
        _filterView = api->CreateView(kFilterViewPath, [](PrismaView v) { RadHud::NoteDomReady(kDomSlotFilter, v); });
        _domLive[kDomSlotFilter].store(_filterView);

        if (_filterView != 0) { setHudViewOrder(_filterView); logger->info("[HUD] Filter widget view created (handle {}).", _filterView); if (!_filterDomReady.load()) { api->Hide(_filterView); _coverHidden[kDomSlotFilter] = 0; } }
    }
    else if (_hudApi)
    {
        _widgetReapplyPending.store(true);
    }

    EnsureClimateView();

    _viewsReady.store(_view != 0 && api->IsValid(_view) &&
                      _toxicView != 0 && api->IsValid(_toxicView) &&
                      _filterView != 0 && api->IsValid(_filterView));
    SyncLocalHudLayout();
}

void RadHud::FinalizeMainDomReady(uint64_t view)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(view)) return;
    api->Show(view);
    PushTheme(view);
    _domReady.store(true, std::memory_order_release);
}

void RadHud::FinalizeDebugDomReady(uint64_t view)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(view)) return;
    api->RegisterJSListener(view, "radDebug", OnRadDebugCommand);
    api->InteropCall(view, "SetDebugMenu", "0");
    PushTheme(view);
    _debugDomReady.store(true, std::memory_order_release);
    SyncLocalHudLayout();
}

void RadHud::FinalizeToxicDomReady(uint64_t view)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(view)) return;
    api->Show(view);
    PushTheme(view);
    if (_hudApi)
    {
        static_cast<PRISMA_HUD_API::IVPrismaHUD1*>(_hudApi)->RegisterWidget(view, CompanionPlaced(kToxicDesc));
        _toxicWidgetRegistered.store(true);
    }
    _toxicDomReady.store(true, std::memory_order_release);
}

void RadHud::FinalizeFilterDomReady(uint64_t view)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(view)) return;
    api->Show(view);
    PushTheme(view);
    if (_hudApi)
        static_cast<PRISMA_HUD_API::IVPrismaHUD1*>(_hudApi)->RegisterWidget(view, CompanionPlaced(kFilterDesc));
    _filterDomReady.store(true, std::memory_order_release);
}

void RadHud::ReapplyWidgetRegistrations()
{
    if (!_api || !_hudApi) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    auto* hud = static_cast<PRISMA_HUD_API::IVPrismaHUD1*>(_hudApi);
    if (_toxicView != 0 && _toxicDomReady.load() && api->IsValid(_toxicView))
    {
        hud->RegisterWidget(_toxicView, CompanionPlaced(kToxicDesc));
        _toxicWidgetRegistered.store(true);
    }
    if (_filterView != 0 && _filterDomReady.load() && api->IsValid(_filterView))
        hud->RegisterWidget(_filterView, CompanionPlaced(kFilterDesc));
    _companionsWidgetRegistered = false;
}

void RadHud::PushToxic(float tissue, float permanentTissue)
{
    if (!_api || !LoadSettled()) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (_view == 0 || !api->IsValid(_view) ||
        _toxicView == 0 || !api->IsValid(_toxicView) ||
        _filterView == 0 || !api->IsValid(_filterView))
    {
        if (IsPlayerInWorld())
        {
            StartPrismaViewRetry();
            if (!_threadApi)
                CreateView();
        }
        return;
    }
    SyncLocalHudLayout();
    if (!IsPlayerInWorld()) return;
    if (_toxicView == 0 || !_toxicDomReady || !api->IsValid(_toxicView)) return;

    const bool companion = IsCompanionMode();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* values = RE::ActorValue::GetSingleton();
    float health = 0.0f, healthMax = 0.0f, rads = 0.0f, ap = 0.0f, apMax = 0.0f;
    const bool fallback = !companion && !_toxicWidgetRegistered.load();
    if (!companion && player && values)
    {
        if (values->health)
        {
            health = player->GetActorValue(*values->health);
            healthMax = EffectiveActorValueMax(*player, *values->health);
            if (healthMax <= 0.0f && health > 0.0f) healthMax = health;
        }
        if (values->rads)
            rads = std::clamp(player->GetActorValue(*values->rads), 0.0f, 1000.0f);
        if (values->actionPoints)
        {
            ap = player->GetActorValue(*values->actionPoints);
            apMax = EffectiveActorValueMax(*player, *values->actionPoints);
            if (apMax <= 0.0f && ap > 0.0f) apMax = ap;
        }
    }

    char json[256];
    std::snprintf(json, sizeof(json),
        "{\"tissue\":%.1f,\"permanentTissue\":%.1f,\"fallback\":%s,"
        "\"health\":%.1f,\"healthMax\":%.1f,\"rads\":%.1f,\"radsMax\":1000.0,"
        "\"ap\":%.1f,\"apMax\":%.1f}",
        tissue, permanentTissue, fallback ? "true" : "false", health, healthMax, rads, ap, apMax);
    if (_toxicPayload == json) return;
    _toxicPayload = json;
    api->InteropCall(_toxicView, "updateToxic", json);
    if (_debugOpen && _debugView && _debugDomReady && api->IsValid(_debugView))
        api->InteropCall(_debugView, "updateVitalsInfo", json);
}

void RadHud::PushMask(bool on)
{
    if (!_api || _view == 0 || !_domReady) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_view)) return;

    if (!IsPlayerInWorld()) return;

    const int state = on ? 1 : 0;
    if (state == _maskPushed) return;
    _maskPushed = state;
    api->InteropCall(_view, "setMask", on ? "{\"on\":true}" : "{\"on\":false}");
}

void RadHud::PushFilter(float charge, float condition, int spares, bool masked)
{
    if (!_api) return;
    if (!IsPlayerInWorld()) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);

    char json[160];
    std::snprintf(json, sizeof(json),
        "{\"charge\":%.0f,\"condition\":%.0f,\"spares\":%d,\"masked\":%s}",
        charge, condition, spares, masked ? "true" : "false");

    if (_view != 0 && _domReady && api->IsValid(_view) && _filterMainPayload != json)
    {
        _filterMainPayload = json;
        api->InteropCall(_view, "updateFilter", json);
    }
    if (_filterView != 0 && _filterDomReady && api->IsValid(_filterView))
    {
        if (_filterBarPayload != json)
        {
            _filterBarPayload = json;
            api->InteropCall(_filterView, "updateFilter", json);
        }
        PushFilterKeys(_filterView);
    }
}

void RadHud::PushAir(const std::string& payload)
{
    if (!_api || !IsPlayerInWorld()) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!_filterView || !_filterDomReady || !api->IsValid(_filterView)) return;
    if (payload == _airPayload) return;
    _airPayload = payload;
    api->InteropCall(_filterView, "updateAir", payload.c_str());
    if (_debugOpen && _debugView && _debugDomReady && api->IsValid(_debugView))
        api->InteropCall(_debugView, "updateAirInfo", payload.c_str());
}

void RadHud::PushGhoulSettings(bool enabled, float chance)
{
    if (!_api || _debugView == 0 || !_debugDomReady) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView)) return;

    char json[96];
    std::snprintf(json, sizeof(json), "{\"enabled\":%s,\"chance\":%.0f}",
        enabled ? "true" : "false", chance * 100.0f);
    api->InteropCall(_debugView, "setGhoulSettings", json);
}

void RadHud::PushNeedsTuning(const std::string& json)
{
    if (!_api || _debugView == 0 || !_debugDomReady) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView)) return;
    api->InteropCall(_debugView, "setNeedsTuning", json.c_str());
}

bool RadHud::ShowSpecialInfo(const std::vector<std::string>& lines)
{
    if (!_api || _debugView == 0 || !_debugDomReady) return false;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView)) return false;

    json j;
    j["lines"] = lines;
    api->InteropCall(_debugView, "showSpecialInfo", j.dump(-1, ' ', false, json::error_handler_t::replace).c_str());
    return true;
}

void RadHud::PushStorm(int phase, int theme, float intensity, bool sheltered, bool lethal)
{
    if (!_api || _view == 0 || !_domReady) return;

    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_view)) return;

    char json[128];
    std::snprintf(json, sizeof(json),
        "{\"phase\":%d,\"theme\":%d,\"intensity\":%.3f,\"sheltered\":%s,\"lethal\":%s}",
        phase, theme, intensity, sheltered ? "true" : "false", lethal ? "true" : "false");
    if (_stormPayload == json) return;
    _stormPayload = json;
    api->InteropCall(_view, "stormPhase", json);
}

void RadHud::PushArmor(
    float insulation01,
    float effectiveInsulation01,
    bool powerArmor,
    const std::vector<ClimateProtectionRuntime::ArmorInfo>& armor)
{
    if (!_api || !_debugOpen || _debugView == 0 || !_debugDomReady)
        return;

    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView))
        return;

    const auto percent = [](float value) {
        if (!std::isfinite(value)) return 0;
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 100.0f));
    };

    json payload{
        { "insulation", percent(insulation01) },
        { "effectiveInsulation", percent(effectiveInsulation01) },
        { "powerArmor", powerArmor },
        { "inPowerArmor", RE::PowerArmor::PlayerInPowerArmor() },
        { "pieces", json::array() }
    };
    for (const auto& piece : armor)
    {
        payload["pieces"].push_back({
            { "formID", piece.formID },
            { "name", RadCorrectnessPolicy::SanitizeUiText(piece.name) },
            { "warmth", percent(piece.warmth01) },
            { "insulation", percent(piece.insulation01) },
            { "override", piece.overridden },
            { "head", piece.head },
            { "torso", piece.torso },
            { "leftArm", piece.leftArm },
            { "rightArm", piece.rightArm },
            { "leftLeg", piece.leftLeg },
            { "rightLeg", piece.rightLeg }
        });
    }

    const std::string serialized = payload.dump(-1, ' ', false, json::error_handler_t::replace);
    if (serialized == _armorPayload)
        return;
    _armorPayload = serialized;
    api->InteropCall(_debugView, "setArmorInfo", _armorPayload.c_str());
}

void RadHud::ResetDebugMenuForSession()
{
    g_inWorld.store(false);
    RadiationEngine::GetSingleton().ResetDebugKeyBaseline();
    _debugOpen = false;
    _debugCloseRequested.store(false);
    _debugTogglePending = false;
    _debugDesiredOpen = false;
    _debugNextFocusRetryMs = 0;
    SetLocalHudEditorPreview(false);
    SetDebugMenu(false);
    HideNeeds();
    HideOutbreak();
    HideCompanions();
}

void RadHud::HideNeeds()
{
    _youDisease = 0;
    _youOnset = 0;
    _needsPayload.clear();
    _coverHidden[kDomSlotNeeds] = 0;
    if (_api && _needsView)
    {
        auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
        if (api->IsValid(_needsView)) api->Hide(_needsView);
    }
}

void RadHud::ToggleDebugMenu()
{
    auto logger = spdlog::get("FalloutAnomaly");
    static ULONGLONG lastToggle = 0; const auto now = ::GetTickCount64(); if (now - lastToggle < 100) return; lastToggle = now;

    if (!_api)
    {
        RE::SendHUDMessage::ShowHUDMessage(
            "[Radiation] Debug menu requires the PrismaUI HUD (not installed). SPECIAL stats are "
            "auto-repaired on each load; use RadAway/sleep to recover.", "", true, true);
        if (logger) logger->warn("[RadHud] ToggleDebugMenu: PrismaUI framework not present.");
        return;
    }

    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);

    if (_debugView != 0 && api->IsValid(_debugView) && _debugDomReady.load())
    {
        if (_debugTogglePending && _debugDesiredOpen)
        {
            _debugTogglePending = false;
            _debugDesiredOpen = false;
            _debugNextFocusRetryMs = 0;
            SetDebugMenu(false);
            if (logger) logger->info("[RadHud] Pending debug-menu open cancelled.");
            return;
        }
        _debugTogglePending = false;
        _debugNextFocusRetryMs = 0;
        SetDebugMenu(!_debugOpen);
        return;
    }

    const bool nextDesiredOpen = _debugTogglePending ? !_debugDesiredOpen : !_debugOpen;
    if (_debugTogglePending && !nextDesiredOpen)
    {
        _debugTogglePending = false;
        _debugDesiredOpen = false;
        _debugNextFocusRetryMs = 0;
        SetDebugMenu(false);
        if (logger) logger->info("[RadHud] Pending debug-menu open cancelled before view readiness.");
        return;
    }
    _debugDesiredOpen   = nextDesiredOpen;
    _debugTogglePending = true;
    if (logger)
        logger->info("[RadHud] Debug-menu toggle deferred until view ready (view={}, domReady={}); desired={}.",
            _debugView, _debugDomReady.load(), _debugDesiredOpen ? "open" : "closed");
}

void RadHud::RequestDebugClose() noexcept
{
    _debugCloseRequested.store(true);
    try
    {
        Hooks::QueueMainThreadTask([]() { RadHud::GetSingleton().PumpDeferredDebug(); });
    }
    catch (...)
    {
    }
}

void RadHud::PumpDeferredDebug()
{
    if (_debugCloseRequested.exchange(false))
    {
        _debugTogglePending = false;
        _debugDesiredOpen = false;
        _debugNextFocusRetryMs = 0;
        SetDebugMenu(false);
        return;
    }
    if (!_debugTogglePending) return;
    if (!_api) { _debugTogglePending = false; return; }

    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (_debugDesiredOpen) EnsureDebugView();
    if (_debugView == 0 || !api->IsValid(_debugView) || !_debugDomReady.load())
        return;

    const auto now = ::GetTickCount64();
    if (_debugDesiredOpen && now < _debugNextFocusRetryMs)
        return;

    SetDebugMenu(_debugDesiredOpen);
    if (_debugOpen == _debugDesiredOpen)
    {
        _debugTogglePending = false;
        _debugNextFocusRetryMs = 0;
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->info("[RadHud] Applied deferred debug-menu toggle: {}.", _debugOpen ? "opened" : "closed");
    }
}

void RadHud::UpdateMenuCover(const char* eventMenu, bool opening)
{
    if (!_api) return;
    static constexpr const char* kCoverMenus[] = {
        "MainMenu", "LoadingMenu", "PauseMenu", "PipboyMenu", "ContainerMenu", "BarterMenu", "TerminalMenu",
        "ExamineMenu", "LockpickingMenu", "LevelUpMenu", "SleepWaitMenu", "LooksMenu", "BookMenu",
        "SPECIALMenu", "CreditsMenu", "CookingMenu", "PowerArmorModMenu" };
    bool covered = false;
    if (auto* ui = RE::UI::GetSingleton())
    {
        for (const char* name : kCoverMenus)
        {
            const bool open = (eventMenu && std::string_view(eventMenu) == name)
                ? opening : ui->GetMenuOpen(RE::BSFixedString(name));
            covered = covered || open;
        }
    }
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    // invariant: views[] is indexed by DOM slot; slot 5 (debug) is never covered
    const uint64_t views[kCoverViewCount] = { _view, _toxicView, _filterView, _climateView, _needsView, 0, _outbreakView, _companionsView };
    if (covered)
    {
        for (int i = 0; i < kCoverViewCount; ++i)
        {
            if (views[i] == 0 || !api->IsValid(views[i]) || api->IsHidden(views[i])) continue;
            api->Hide(views[i]);
            _coverHidden[i] = views[i];
        }
    }
    else if (_menuCovered)
    {
        for (int i = 0; i < kCoverViewCount; ++i)
        {
            if (_coverHidden[i] != 0 && _coverHidden[i] == views[i] && api->IsValid(views[i])) api->Show(views[i]);
            _coverHidden[i] = 0;
        }
    }
    _menuCovered = covered;
}

void RadHud::OnBlockingMenuOpened()
{
    _debugTogglePending = false;
    _debugDesiredOpen = false;
    _debugNextFocusRetryMs = 0;
    if (_debugOpen || _debugView != 0)
        SetDebugMenu(false);
}

namespace
{
    void SetDebugViewOwnsEscape(uint64_t view, bool owns)
    {
        if (auto* escape = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI9>())
            escape->SetViewOwnsEscape(view, owns);
    }

    constexpr ULONGLONG kDebugFocusRetryMs = 150;
    constexpr ULONGLONG kDebugFocusGiveUpMs = 2000;

    void SetDebugViewRole(uint64_t view, PRISMA_UI_API::ViewRole role)
    {
        if (auto* roles = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>())
            roles->SetViewRole(view, role);
    }

    bool FocusDebugOverlayFallback(uint64_t view)
    {
        auto* overlay = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
        return overlay != nullptr && overlay->FocusOverlay(view, false, false);
    }
}

void RadHud::EnsureDebugView()
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (_debugView != 0 && api->IsValid(_debugView)) return;
    if (!LoadSettled() || !PlayerInWorld()) return;

    _debugDomReady = false;
    _debugPushedOpen = -1;
    _armorPayload.clear();
    _survivalPayload.clear();
    _outbreakDebugPayload.clear();
    _domLive[kDomSlotDebug].store(0);
    _debugView = api->CreateView(kDebugViewPath, [](PrismaView v) { RadHud::NoteDomReady(kDomSlotDebug, v); });
    _domLive[kDomSlotDebug].store(_debugView);
    if (_debugView == 0)
    {
        _debugTogglePending = false;
        _debugDesiredOpen = false;
        return;
    }
    if (!_debugDomReady.load()) api->Hide(_debugView);
}

void RadHud::DestroyDebugView()
{
    RadiationEngine::GetSingleton().DrainDebugCommands();
    RadiationEngine::GetSingleton().ResumeDebugTime();
    SetLocalHudEditorPreview(false);
    if (_debugView == 0) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    api->Unfocus(_debugView);
    SetDebugViewOwnsEscape(_debugView, false);
    SetDebugViewRole(_debugView, PRISMA_UI_API::ViewRole::kWidget);
    _domLive[kDomSlotDebug].store(0);
    api->Destroy(_debugView);
    _debugView = 0;
    _debugDomReady = false;
    _debugPushedOpen = -1;
    InvalidateLocalHudLayoutViews();
}

void RadHud::ReleaseStaleDebugFocus() noexcept
{
    if (_debugOpen || _api == nullptr || _debugView == 0) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView) || !api->HasFocus(_debugView)) return;
    api->Unfocus(_debugView);
    SetDebugViewOwnsEscape(_debugView, false);
}

void RadHud::PushDebugPanelState(bool open) noexcept
{
    if (_api == nullptr || _debugView == 0 || !_debugDomReady.load()) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);
    if (!api->IsValid(_debugView)) return;
    api->InteropCall(_debugView, "SetDebugMenu", open ? "1" : "0");
    _debugPushedOpen = open ? 1 : 0;
}

void RadHud::SyncDebugPanelState() noexcept
{
    if (_debugView == 0) return;
    if (_debugPushedOpen == (_debugOpen ? 1 : 0)) return;
    PushDebugPanelState(_debugOpen);
}

void RadHud::SetDebugMenu(bool open)
{
    if (!_api) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI2*>(_api);

    if (!open)
    {
        _debugOpen = false;
        _debugTogglePending = false;
        _debugDesiredOpen = false;
        _debugNextFocusRetryMs = 0;
        _debugFocusFailedSinceMs = 0;
        PushDebugPanelState(false);
        DestroyDebugView();
    }
    else
    {
        if (_debugView == 0 || !_debugDomReady.load() || !api->IsValid(_debugView)) return;
        if (!_climatePayload.empty()) api->InteropCall(_debugView, "updateClimateInfo", _climatePayload.c_str());
        if (!_airPayload.empty()) api->InteropCall(_debugView, "updateAirInfo", _airPayload.c_str());
        if (!_toxicPayload.empty()) api->InteropCall(_debugView, "updateVitalsInfo", _toxicPayload.c_str());
        auto& climate = ClimateRuntime::GetSingleton();
        PushArmor(climate.LastInsulation01(), climate.LastEffectiveInsulation01(),
            climate.LastPowerArmor(), climate.LastArmor());
        api->Show(_debugView);
        bool focusAccepted = api->Focus(_debugView, false, false);
        if (!focusAccepted) focusAccepted = FocusDebugOverlayFallback(_debugView);
        if (!focusAccepted)
        {
            _debugOpen = false;
            PushDebugPanelState(false);
            api->Unfocus(_debugView);
            SetDebugViewOwnsEscape(_debugView, false);
            SetDebugViewRole(_debugView, PRISMA_UI_API::ViewRole::kWidget);

            const auto now = ::GetTickCount64();
            if (_debugNextFocusRetryMs == 0 || _debugFocusFailedSinceMs == 0)
                _debugFocusFailedSinceMs = now;
            if (now - _debugFocusFailedSinceMs >= kDebugFocusGiveUpMs)
            {
                _debugDesiredOpen = false;
                _debugTogglePending = false;
                _debugNextFocusRetryMs = 0;
                _debugFocusFailedSinceMs = 0;
                DestroyDebugView();
                RE::SendHUDMessage::ShowHUDMessage(
                    "[Radiation] Debug menu could not take focus. Press Shift+F8 to try again.", "", true, true);
                return;
            }
            _debugDesiredOpen = true;
            _debugTogglePending = true;
            _debugNextFocusRetryMs = now + kDebugFocusRetryMs;

            static ULONGLONG lastFocusFailureLog = 0;
            if (now - lastFocusFailureLog >= 1000)
            {
                lastFocusFailureLog = now;
                if (auto logger = spdlog::get("FalloutAnomaly"))
                    logger->warn("[RadHud] Debug-menu focus was rejected; retrying while the view settles.");
            }
            return;
        }

        _debugOpen = true;
        _debugTogglePending = false;
        _debugNextFocusRetryMs = 0;
        _debugFocusFailedSinceMs = 0;
        SetDebugViewOwnsEscape(_debugView, true);
        SetDebugViewRole(_debugView, PRISMA_UI_API::ViewRole::kPanel);
        PushDebugPanelState(true);
        RadiationEngine::GetSingleton().PushGhoulSettingsToHud();
        PushNeedsTuning(RadNeedsTuning::Json());
    }

    spdlog::get("FalloutAnomaly")->info("[HUD] Debug menu {}.", open ? "opened" : "closed");
}
