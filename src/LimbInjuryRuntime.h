#pragma once

#include "LimbInjuryPolicy.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace RE
{
    class PlayerCharacter;
    class TESGlobal;
}

class LimbInjuryRuntime
{
public:
    struct HudSnapshot
    {
        LimbInjuryPolicy::LimbStates         states{};
        std::array<float, LimbInjuryPolicy::kLimbCount> condition{};
        LimbInjuryPolicy::RegionLevelSet     fx{};
    };

    static LimbInjuryRuntime& GetSingleton()
    {
        static LimbInjuryRuntime instance;
        return instance;
    }

    void Update();
    void Reset();
    void HandleDebug(const std::string& a_command);

    HudSnapshot GetHudSnapshot() const;

    void OnPlayerHit(LimbInjuryPolicy::HitCause a_cause);
    void OnItemEquipped(std::uint32_t a_formID);
    void OnItemRemoved(std::uint32_t a_formID);

private:
    using Clock = std::chrono::steady_clock;

    LimbInjuryRuntime() = default;

    void EnsureSinks();
    void ResolveForms();
    void LoadFromGlobals();
    void Commit(bool a_enabled);
    void ReadConditions(RE::PlayerCharacter* a_player, std::array<float, LimbInjuryPolicy::kLimbCount>& a_out) const;
    void ProcessItems(RE::PlayerCharacter* a_player, bool a_crippleOn);
    void AfterStimpak(RE::PlayerCharacter* a_player, bool a_crippleOn);
    void ApplyDrops(const std::array<float, LimbInjuryPolicy::kLimbCount>& a_current, double a_now);
    void ApplyBleed(RE::PlayerCharacter* a_player, float a_dtReal);
    void HoldCripple(RE::PlayerCharacter* a_player, bool a_enabled);
    void ReleaseHeld(RE::PlayerCharacter* a_player, std::size_t a_limb);
    bool CheckDoctor(RE::PlayerCharacter* a_player, const std::array<float, LimbInjuryPolicy::kLimbCount>& a_current, float a_health);
    static double NowSeconds();

    mutable std::mutex _mutex;
    mutable std::mutex _hudMutex;
    std::mutex         _eventMutex;

    bool _sinksRegistered{ false };
    bool _loaded{ false };
    bool _havePrev{ false };
    bool _stimNoticeShown{ false };

    std::array<RE::TESGlobal*, LimbInjuryPolicy::kLimbCount> _limbGlobals{};
    std::array<RE::TESGlobal*, 4>                           _fxGlobals{};
    std::array<std::uint32_t, 5>                            _itemIds{};
    std::uint32_t                                           _stimpakId{ 0 };
    bool                                                    _formsResolved{ false };

    LimbInjuryPolicy::LimbStates                            _states{};
    std::array<float, LimbInjuryPolicy::kLimbCount>         _clot{};
    std::array<float, LimbInjuryPolicy::kLimbCount>         _held{};
    std::array<float, LimbInjuryPolicy::kLimbCount>         _prev{};
    float                                                   _prevHealth{ 0.0f };
    double                                                  _lastReal{ 0.0 };
    float                                                   _lastHours{ 0.0f };
    bool                                                    _haveHours{ false };
    double                                                  _lastAirSec{ -1.0e9 };
    bool                                                    _airSeen{ false };

    std::atomic<double>                                     _lastHitSec{ -1.0e9 };
    std::atomic<int>                                        _lastHitCause{ 0 };

    std::vector<std::pair<std::uint32_t, double>>           _equipped;
    std::vector<std::pair<std::uint32_t, double>>           _removed;
    std::vector<std::uint32_t>                              _usedItems;

    HudSnapshot                                             _hud{};
};
