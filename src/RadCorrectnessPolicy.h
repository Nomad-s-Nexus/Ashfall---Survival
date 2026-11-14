#pragma once

#include "RadIniParse.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace RadCorrectnessPolicy
{
    struct Position
    {
        float x;
        float y;
        float z;

        auto operator==(const Position&) const -> bool = default;
    };

    struct NukePlacement
    {
        Position strike;
        float facingYaw;
    };

    inline NukePlacement DistantNukePlacementXY(Position player, float originYaw, float distance)
    {
        const Position strike{
            player.x + distance * std::sin(originYaw),
            player.y + distance * std::cos(originYaw),
            0.0f
        };
        return { strike, std::atan2(player.x - strike.x, player.y - strike.y) };
    }

    constexpr float StormShakeStrength(float base, bool interior)
    {
        return interior ? base * 0.35f : base;
    }

    constexpr bool ShouldReturnToClear(bool managedStorm, bool vanillaRadstorm)
    {
        return managedStorm || vanillaRadstorm;
    }

    constexpr bool UsesVanillaRadstormSky(int typeIndex)
    {
        return typeIndex == 0 || typeIndex == 2;
    }

    inline constexpr int kPhaseIdle = 0;
    inline constexpr int kPhaseWarning = 1;
    inline constexpr int kPhaseActive = 2;
    inline constexpr int kPhaseAftermath = 3;
    inline constexpr int kPhaseApproach = 4;

    inline constexpr int kStormSkyTypeCount = 5;
    inline constexpr int kStormSkyDustType = 4;

    enum class StormSkyKind
    {
        None,
        VanillaRadstorm,
        CustomWeather
    };

    constexpr StormSkyKind StormSkyChoice(int typeIndex, int phase)
    {
        if (typeIndex < 0 || typeIndex >= kStormSkyTypeCount)
            return StormSkyKind::None;
        if (phase == kPhaseActive)
            return UsesVanillaRadstormSky(typeIndex) ? StormSkyKind::VanillaRadstorm : StormSkyKind::CustomWeather;
        if (phase == kPhaseWarning && typeIndex == kStormSkyDustType)
            return StormSkyKind::CustomWeather;
        return StormSkyKind::None;
    }

    constexpr bool SpawnGhoulsOnRadWeatherEntry(bool entered, bool vanillaRadstorm, bool debugHold, int phase)
    {
        return entered && (!vanillaRadstorm || debugHold || phase == kPhaseActive);
    }

    constexpr bool VanillaSkyStormBedAllowed(int typeIndex, int phase)
    {
        return StormSkyChoice(typeIndex, phase) == StormSkyKind::VanillaRadstorm;
    }

    constexpr bool InVanillaRadstormCooldown(double now, double last, double gapSec)
    {
        return (now - last) < gapSec;
    }

    constexpr bool ShouldStampRadstormCooldown(int typeIndex, int phaseLeaving)
    {
        return UsesVanillaRadstormSky(typeIndex) && phaseLeaving == kPhaseActive;
    }

    inline float ResolveStormLeadMinutes(std::string_view stormKey, std::string_view domeKey)
    {
        constexpr float kDefaultLeadMinutes = 3.0f;
        std::string_view key = stormKey.empty() ? domeKey : stormKey;
        if (key.empty())
            return kDefaultLeadMinutes;
        key = key.substr(0, key.find(';'));
        const std::string text(key);
        char* end = nullptr;
        const float value = std::strtof(text.c_str(), &end);
        if (end == text.c_str())
            return kDefaultLeadMinutes;
        while (*end == ' ' || *end == '\t')
            ++end;
        if (*end != '\0')
            return kDefaultLeadMinutes;
        if (!std::isfinite(value) || value < 0.0f)
            return kDefaultLeadMinutes;
        return value;
    }

    constexpr bool IsGasMask(bool fillsMouth, bool hasKeyword)
    {
        return fillsMouth && hasKeyword;
    }

    constexpr bool IsConfiguredMask(bool listed, bool fillsMouth, bool hasKeyword)
    {
        return listed || IsGasMask(fillsMouth, hasKeyword);
    }

    constexpr bool IsRadShieldedMask(bool fillsMouth, bool radResistant)
    {
        return fillsMouth && radResistant;
    }

    constexpr bool MaskOverlayVisible(int a_debugOverride, bool a_wornMask, bool a_powerArmorHelmet, bool a_firstPerson)
    {
        if (a_debugOverride == 1)
            return true;
        if (a_debugOverride == 0)
            return false;
        return a_wornMask && !a_powerArmorHelmet && a_firstPerson;
    }

    constexpr bool RadWeatherExposed(
        bool a_inRadWeather,
        bool a_interior,
        bool a_sheltered,
        bool a_poweredShelter,
        bool a_powerArmor)
    {
        return a_inRadWeather && !a_interior && !a_sheltered && !a_poweredShelter && !a_powerArmor;
    }

    inline constexpr int kAutoMaskStableTicks = 12;

    inline constexpr int kChokeGraceTicks = kAutoMaskStableTicks + 1;

    struct AutoMaskStep
    {
        bool equip;
        bool remove;
        int  applied;
        int  candidate;
        int  streak;
    };

    constexpr AutoMaskStep StepAutoMask(
        int  a_applied,
        int  a_candidate,
        int  a_streak,
        bool a_exposedNow,
        int  a_requiredStableTicks)
    {
        const int sample = a_exposedNow ? 1 : 0;
        if (a_applied < 0)
            return { false, false, sample, sample, 0 };
        int candidate = a_candidate;
        int streak = a_streak;
        if (candidate == sample)
            ++streak;
        else
        {
            candidate = sample;
            streak = 1;
        }
        if (streak < a_requiredStableTicks || candidate == a_applied)
            return { false, false, a_applied, candidate, streak };
        return { candidate == 1, candidate == 0, candidate, candidate, 0 };
    }

    inline bool ParseHotkey(std::string_view a_text, int& a_out)
    {
        a_text = a_text.substr(0, a_text.find(';'));
        std::uint32_t value = 0;
        if (!RadIniParse::ParseNumber(a_text, 0xFFF, value))
            return false;
        a_out = static_cast<int>(value);
        return true;
    }

    constexpr bool IsUsableHotkey(int a_key)
    {
        if (a_key == 0)
            return true;
        if (a_key < 1 || a_key > 0xFF)
            return false;
        if (a_key <= 6)
            return false;
        return a_key != 0x10 && a_key != 0x1B && a_key != 0x77 && a_key != 0xA0 && a_key != 0xA1;
    }

    struct HotkeyPair
    {
        int  replace;
        int  repair;
        bool replaceRejected;
        bool repairRejected;
        bool duplicateRejected;
    };

    constexpr HotkeyPair ValidateHotkeyPair(int a_replace, int a_repair)
    {
        HotkeyPair result{ a_replace, a_repair, false, false, false };
        if (!IsUsableHotkey(a_replace))
        {
            result.replace = 0;
            result.replaceRejected = true;
        }
        if (!IsUsableHotkey(a_repair))
        {
            result.repair = 0;
            result.repairRejected = true;
        }
        if (result.replace != 0 && result.replace == result.repair)
        {
            result.replace = 0;
            result.repair = 0;
            result.duplicateRejected = true;
        }
        return result;
    }

    enum class CureSignal
    {
        kNone,
        kInventoryMovement,
        kVerifiedChelationUse,
        kVerifiedRadAwayUse
    };

    constexpr bool CuresPermanentTissue(CureSignal signal)
    {
        return signal == CureSignal::kVerifiedChelationUse;
    }

    constexpr bool ArmsRadAwayHealing(CureSignal signal)
    {
        return signal == CureSignal::kVerifiedRadAwayUse;
    }

    constexpr bool IsEngineActive(bool coreInitialized, bool updateProducerInstalled)
    {
        return coreInitialized && updateProducerInstalled;
    }

    constexpr bool IsCurrentLoadGeneration(std::uint64_t expected, std::uint64_t actual)
    {
        return expected == actual;
    }

    constexpr bool ShouldStartHudWait(bool alreadyWaiting, std::uint64_t expectedLoad, std::uint64_t actualLoad)
    {
        return !alreadyWaiting && IsCurrentLoadGeneration(expectedLoad, actualLoad);
    }

    inline std::uint64_t BeginNewGameLoad(
        std::atomic<std::uint64_t>& loadGeneration,
        std::atomic<bool>& hudWaiting)
    {
        const auto nextGeneration = loadGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        hudWaiting.store(false, std::memory_order_release);
        return nextGeneration;
    }

    constexpr float kGodModeFloorHealth = 1.0f;
    constexpr float GodModeClampedDrain(float currentHealth, float kDamage, bool godMode, bool respectGodMode)
    {
        if (!godMode || !respectGodMode || !(kDamage < 0.0f))
            return kDamage;
        const float room = currentHealth - kGodModeFloorHealth;
        if (!(room > 0.0f))
            return 0.0f;
        const float take = (-kDamage < room) ? -kDamage : room;
        return -take;
    }

    inline std::string SanitizeUiText(std::string_view a_text)
    {
        std::string out;
        out.reserve(a_text.size());
        std::size_t i = 0;
        while (i < a_text.size())
        {
            const auto lead = static_cast<unsigned char>(a_text[i]);
            if (lead < 0x80)
            {
                out.push_back((lead < 0x20 || lead == 0x7F) ? '?' : static_cast<char>(lead));
                ++i;
                continue;
            }
            std::size_t len = 0;
            unsigned char lo = 0x80;
            unsigned char hi = 0xBF;
            if (lead >= 0xC2 && lead <= 0xDF)
                len = 2;
            else if (lead >= 0xE0 && lead <= 0xEF)
            {
                len = 3;
                if (lead == 0xE0) lo = 0xA0;
                if (lead == 0xED) hi = 0x9F;
            }
            else if (lead >= 0xF0 && lead <= 0xF4)
            {
                len = 4;
                if (lead == 0xF0) lo = 0x90;
                if (lead == 0xF4) hi = 0x8F;
            }
            if (len == 0)
            {
                out.push_back('?');
                ++i;
                continue;
            }
            std::size_t valid = 1;
            while (valid < len && i + valid < a_text.size())
            {
                const auto next = static_cast<unsigned char>(a_text[i + valid]);
                const unsigned char low = valid == 1 ? lo : 0x80;
                const unsigned char high = valid == 1 ? hi : 0xBF;
                if (next < low || next > high)
                    break;
                ++valid;
            }
            if (valid < len || len == 4)
            {
                out.push_back('?');
                i += valid;
                continue;
            }
            out.append(a_text.substr(i, len));
            i += len;
        }
        return out;
    }

    inline constexpr std::uint64_t kEngineTickMs = 250;
    inline constexpr std::uint64_t kLoadSettleMs = 3000;
    inline constexpr std::uint64_t kCompanionBridgeMaxTicks = 40;

    constexpr bool LoadSettleElapsed(std::uint64_t nowMs, std::uint64_t loadMs, std::uint64_t settleMs)
    {
        return nowMs >= loadMs && nowMs - loadMs >= settleMs;
    }

    constexpr bool CompanionBridgeReady(bool companion, bool phosphorViewsSeen, std::uint64_t ticksSinceLoad)
    {
        return !companion || phosphorViewsSeen || ticksSinceLoad >= kCompanionBridgeMaxTicks;
    }

    constexpr bool IsLiveDomReady(std::uint64_t pendingView, std::uint64_t liveView)
    {
        return pendingView != 0 && pendingView == liveView;
    }
}
