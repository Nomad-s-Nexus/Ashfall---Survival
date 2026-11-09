#pragma once

#include "ClimateProtectionRuntime.h"
#include "NeedsPolicy.h"
#include "RadCorrectnessPolicy.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace PRISMA_HUD_API { struct WidgetDesc; }

bool CompanionWidgetDefault(const char* id, float& x, float& y);
PRISMA_HUD_API::WidgetDesc CompanionPlaced(const PRISMA_HUD_API::WidgetDesc& desc);

class RadHud
{
public:
    static RadHud& GetSingleton()
    {
        static RadHud instance;
        return instance;
    }

    void RequestAPI();
    bool StartPrismaViewRetry();
    void CreateView();

    void PushToxic(float tissue, float permanentTissue);
    void PushMask(bool on);
    void PushFilter(float charge, float condition, int spares, bool masked);
    void PushClimate(const RadClimatePolicy::ClimateState& state,
        const RadClimatePolicy::EnvironmentSample& environment, const RadClimatePolicy::StepResult& result);
    void PushAir(const std::string& payload);
    void PushNeeds(const RadNeedsPolicy::NeedsSnapshot& snapshot);
    void HideNeeds();
    void PushSurvival(const RadNeedsPolicy::NeedsSnapshot& snapshot);
    void PushArmor(float insulation01, float effectiveInsulation01, bool powerArmor,
                   const std::vector<ClimateProtectionRuntime::ArmorInfo>& armor);
    void PushGhoulSettings(bool enabled, float chance);
    void PushNeedsTuning(const std::string& json);
    void ResetClimateForSession() noexcept;
    void PushTheme(uint64_t view);

    void BeginLoadSettle() noexcept;
    void TickLoadSettle();
    [[nodiscard]] bool LoadSettled() const noexcept;
    [[nodiscard]] bool BridgeReady();
    [[nodiscard]] bool PrismaWindowOpen();
    static constexpr int kDomSlotMain = 0;
    static constexpr int kDomSlotToxic = 1;
    static constexpr int kDomSlotFilter = 2;
    static constexpr int kDomSlotClimate = 3;
    static constexpr int kDomSlotNeeds = 4;
    static constexpr int kDomSlotDebug = 5;
    static constexpr int kDomSlotCount = 6;
    static void NoteDomReady(int slot, uint64_t view) noexcept;
    void PumpDomReady();

    bool IsCompanionMode();
    bool ShowSpecialInfo(const std::vector<std::string>& lines);
    void PushStorm(int phase, int theme, float intensity, bool sheltered, bool lethal);

    void ToggleDebugMenu();
    void SetDebugMenu(bool open);
    void RequestDebugClose() noexcept;
    [[nodiscard]] bool DebugMenuOpen() const noexcept { return _debugOpen; }
    void OpenHudLayoutEditor();
    void ApplyLocalHudLayout(const std::string& payload);
    void SyncLocalHudLayout();
    void SetLocalHudEditorPreview(bool on);
    void InvalidateLocalHudLayoutViews();
    void ResetDebugMenuForSession();
    void PumpDeferredDebug();
    void ReleaseStaleDebugFocus() noexcept;
    void SyncDebugPanelState() noexcept;
    void OnBlockingMenuOpened();
    void UpdateMenuCover(const char* eventMenu = nullptr, bool opening = false);
    [[nodiscard]] bool DebugWorkPending() const noexcept { return _debugCloseRequested.load() || _debugTogglePending.load(); }

    [[nodiscard]] bool PlayerInWorld() const;
    [[nodiscard]] bool ViewsExist() const;
    [[nodiscard]] bool Available() const { return _api != nullptr; }
    [[nodiscard]] bool HudLayoutAvailable() const { return _hudApi != nullptr; }
    [[nodiscard]] bool ViewsReady() const
    {
        return _viewsReady.load() && _domReady.load() && _toxicDomReady.load() && _filterDomReady.load();
    }

private:
    RadHud() = default;
    ~RadHud() = default;

    void EnsureClimateView();
    void PushFilterKeys(uint64_t view);
    void FinalizeMainDomReady(uint64_t view);
    void FinalizeToxicDomReady(uint64_t view);
    void FinalizeFilterDomReady(uint64_t view);
    void FinalizeClimateDomReady(uint64_t view);
    void FinalizeNeedsDomReady(uint64_t view);
    void ReapplyWidgetRegistrations();
    bool ScanPhosphorViews();
    void EnsureDebugView();
    void DestroyDebugView();
    void FinalizeDebugDomReady(uint64_t view);

    static inline std::atomic<uint64_t> _domPending[kDomSlotCount]{};
    static inline std::atomic<uint64_t> _domLive[kDomSlotCount]{};
    std::atomic<uint64_t> _ticksSinceLoad{ 0 };
    std::atomic<bool> _phosphorViewsSeen{ false };
    void* _enumApi = nullptr;
    std::atomic<bool> _widgetReapplyPending{ false };
    static constexpr int kCoverViewCount = 5;
    uint64_t _coverHidden[kCoverViewCount]{};
    bool _menuCovered = false;

    void* _api = nullptr;
    void* _threadApi = nullptr;
    std::atomic<bool> _viewCreateRetry{ false };
    std::atomic<std::uint64_t> _viewRetryNotBeforeMs{ 0 };
    std::atomic<bool> _viewsReady{ false };
    uint64_t _view = 0;
    std::atomic<bool> _domReady{ false };

    void* _hudApi = nullptr;
    void* _hudEditorApi = nullptr;
    uint64_t _toxicView = 0;
    std::atomic<bool> _toxicDomReady{ false };
    std::atomic<bool> _toxicWidgetRegistered{ false };

    uint64_t _filterView = 0;
    std::atomic<bool> _filterDomReady{ false };

    uint64_t _climateView = 0;
    std::atomic<bool> _climateDomReady{ false };
    std::atomic<bool> _climateWidgetRegistered{ false };
    std::string _climatePayload;
    std::string _airPayload;
    std::string _toxicPayload;
    std::string _armorPayload;
    std::string _filterMainPayload;
    std::string _filterBarPayload;
    std::string _stormPayload;
    int _keysReplaceSent = -2;
    int _keysRepairSent = -2;
    std::atomic<int> _companionMode{ -1 };
    uint64_t _needsView = 0;
    std::atomic<uint64_t> _needsReadyView{ 0 };
    bool _needsWidgetRegistered = false;
    std::string _needsPayload;
    std::string _survivalPayload;

    int _maskPushed = -1;

    bool _debugOpen = false;
    uint64_t _debugView = 0;
    std::atomic<bool> _debugDomReady{ false };
    int _debugPushedOpen = -1;
    void PushDebugPanelState(bool open) noexcept;
    std::atomic_bool _debugCloseRequested{ false };
    std::atomic_bool _debugTogglePending{ false };
    bool _debugDesiredOpen = false;

    std::uint64_t _debugNextFocusRetryMs = 0;
    std::uint64_t _debugFocusFailedSinceMs = 0;
};
