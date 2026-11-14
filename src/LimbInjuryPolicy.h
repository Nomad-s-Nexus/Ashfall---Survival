#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace LimbInjuryPolicy
{
    enum Limb : std::size_t
    {
        kHead, kTorso, kLeftArm, kRightArm, kLeftLeg, kRightLeg,
        kLimbCount
    };

    enum class HitCause { Unknown, Melee, Ballistic, Explosion, Fall };

    enum TreatKind : int
    {
        kTreatNone, kTreatBandage, kTreatSplint, kTreatTourniquet, kTreatSutureKit, kTreatMedKit,
        kTreatKindCount
    };

    struct LimbState
    {
        int   trauma{ 0 };
        int   bleed{ 0 };
        bool  splinted{ false };
        bool  tourniquet{ false };
        float heal{ 0.0f };
    };

    using LimbStates = std::array<LimbState, kLimbCount>;

    struct HitResult
    {
        int trauma{ 0 };
        int bleed{ 0 };
    };

    struct RegionLevelSet
    {
        int legs{ 0 };
        int arms{ 0 };
        int head{ 0 };
        int torso{ 0 };
    };

    inline constexpr float kBruiseScore = 8.0f;
    inline constexpr float kSprainScore = 20.0f;
    inline constexpr float kFractureScore = 40.0f;
    inline constexpr float kLightBleedScore = 15.0f;
    inline constexpr float kHeavyBleedScore = 35.0f;
    inline constexpr float kMeleeBleedOffset = 10.0f;
    inline constexpr float kBruiseHours = 6.0f;
    inline constexpr float kSprainHours = 18.0f;
    inline constexpr float kSprainSupportedHours = 9.0f;
    inline constexpr float kFractureHours = 48.0f;
    inline constexpr float kRestMultiplier = 2.0f;
    inline constexpr float kClotSeconds = 90.0f;
    inline constexpr float kTreatHeal = 0.25f;
    inline constexpr float kMaxHeal = 0.99f;

    inline bool IsArm(std::size_t a_limb) { return a_limb == kLeftArm || a_limb == kRightArm; }
    inline bool IsLeg(std::size_t a_limb) { return a_limb == kLeftLeg || a_limb == kRightLeg; }
    inline bool IsLimb(std::size_t a_limb) { return IsArm(a_limb) || IsLeg(a_limb); }

    inline float Encode(const LimbState& a_state)
    {
        const int   trauma = std::clamp(a_state.trauma, 0, 3);
        const int   bleed = std::clamp(a_state.bleed, 0, 2);
        const int   flags = (a_state.splinted ? 1 : 0) | (a_state.tourniquet ? 2 : 0);
        const float heal = std::isfinite(a_state.heal) ? std::clamp(a_state.heal, 0.0f, kMaxHeal) : 0.0f;
        return static_cast<float>(flags * 100 + trauma * 10 + bleed) + heal;
    }

    inline LimbState Decode(float a_value)
    {
        LimbState state;
        if (!std::isfinite(a_value) || a_value < 0.0f || a_value >= 400.0f)
            return state;
        const int whole = static_cast<int>(std::floor(a_value));
        const int flags = whole / 100;
        const int trauma = (whole / 10) % 10;
        const int bleed = whole % 10;
        if (trauma > 3 || bleed > 2)
            return state;
        state.trauma = trauma;
        state.bleed = bleed;
        state.splinted = (flags & 1) != 0;
        state.tourniquet = (flags & 2) != 0;
        state.heal = std::clamp(a_value - static_cast<float>(whole), 0.0f, kMaxHeal);
        return state;
    }

    inline HitResult ClassifyHit(float a_score, HitCause a_cause, bool a_crippled, std::size_t a_limb)
    {
        HitResult result;
        if (!std::isfinite(a_score))
            a_score = 0.0f;
        if (a_score >= kFractureScore)
            result.trauma = 3;
        else if (a_score >= kSprainScore)
            result.trauma = 2;
        else if (a_score >= kBruiseScore)
            result.trauma = 1;

        if (a_crippled)
        {
            result.trauma = std::max(result.trauma, 2);
            if ((a_cause == HitCause::Explosion || a_cause == HitCause::Fall) && IsLimb(a_limb))
                result.trauma = 3;
        }

        float lightAt = 0.0f;
        float heavyAt = 0.0f;
        bool  bleeds = true;
        switch (a_cause)
        {
        case HitCause::Ballistic:
        case HitCause::Explosion:
            lightAt = kLightBleedScore;
            heavyAt = kHeavyBleedScore;
            break;
        case HitCause::Melee:
            lightAt = kLightBleedScore + kMeleeBleedOffset;
            heavyAt = kHeavyBleedScore + kMeleeBleedOffset;
            break;
        default:
            bleeds = false;
            break;
        }
        if (bleeds)
            result.bleed = a_score >= heavyAt ? 2 : (a_score >= lightAt ? 1 : 0);
        return result;
    }

    inline LimbState ApplyHit(LimbState a_state, const HitResult& a_hit)
    {
        if (a_hit.trauma > 0)
        {
            int next = a_state.trauma;
            if (a_hit.trauma > a_state.trauma)
                next = a_hit.trauma;
            else if (a_hit.trauma == a_state.trauma)
                next = std::min(a_state.trauma + 1, 3);
            if (next != a_state.trauma)
            {
                a_state.trauma = next;
                a_state.splinted = false;
                a_state.heal = 0.0f;
            }
        }
        if (a_hit.bleed > 0 && !a_state.tourniquet)
            a_state.bleed = std::max(a_state.bleed, a_hit.bleed);
        return a_state;
    }

    inline int EffectiveLevel(const LimbState& a_state)
    {
        int level = a_state.trauma;
        if (a_state.splinted && a_state.trauma >= 2)
            --level;
        if (a_state.tourniquet)
            ++level;
        return std::max(level, 0);
    }

    inline RegionLevelSet RegionLevels(const LimbStates& a_states)
    {
        const auto level = [&](std::size_t a_limb) { return EffectiveLevel(a_states[a_limb]); };
        const auto pair = [&](std::size_t a_left, std::size_t a_right) {
            const int left = level(a_left);
            const int right = level(a_right);
            return std::min(std::max(left, right) + ((left >= 2 && right >= 2) ? 1 : 0), 4);
        };
        RegionLevelSet set;
        set.legs = pair(kLeftLeg, kRightLeg);
        set.arms = pair(kLeftArm, kRightArm);
        set.head = std::min(level(kHead), 3);
        set.torso = std::min(level(kTorso), 3);
        return set;
    }

    inline float TierHours(const LimbState& a_state)
    {
        switch (a_state.trauma)
        {
        case 1: return kBruiseHours;
        case 2: return a_state.splinted ? kSprainSupportedHours : kSprainHours;
        case 3: return a_state.splinted ? kFractureHours : 0.0f;
        default: return 0.0f;
        }
    }

    inline void StepDown(LimbState& a_state)
    {
        if (a_state.trauma <= 0)
            return;
        --a_state.trauma;
        a_state.heal = 0.0f;
        if (a_state.trauma == 0)
            a_state.splinted = false;
    }

    inline void AddProgress(LimbState& a_state, float a_fraction)
    {
        if (a_state.trauma <= 0 || TierHours(a_state) <= 0.0f)
            return;
        a_state.heal += a_fraction;
        if (a_state.heal >= 1.0f)
        {
            const float carry = a_state.heal - 1.0f;
            StepDown(a_state);
            a_state.heal = a_state.trauma > 0 ? std::min(carry, kMaxHeal) : 0.0f;
        }
        a_state.heal = std::min(a_state.heal, kMaxHeal);
    }

    inline void Heal(LimbState& a_state, float a_dtHours, bool a_rest, float a_rate, float a_realSeconds, float& a_clotSeconds)
    {
        if (a_state.bleed == 1 && !a_state.tourniquet)
        {
            if (std::isfinite(a_realSeconds) && a_realSeconds > 0.0f)
                a_clotSeconds += a_realSeconds;
            if (a_clotSeconds >= kClotSeconds)
            {
                a_state.bleed = 0;
                a_clotSeconds = 0.0f;
            }
        }
        else
        {
            a_clotSeconds = 0.0f;
        }

        if (!std::isfinite(a_dtHours) || !std::isfinite(a_rate) || a_dtHours <= 0.0f || a_rate <= 0.0f)
            return;
        float hours = a_dtHours * a_rate * (a_rest ? kRestMultiplier : 1.0f);
        while (hours > 0.0f && a_state.trauma > 0)
        {
            const float duration = TierHours(a_state);
            if (duration <= 0.0f)
                break;
            const float need = (1.0f - a_state.heal) * duration;
            if (hours < need)
            {
                a_state.heal += hours / duration;
                break;
            }
            hours -= need;
            StepDown(a_state);
        }
    }

    inline bool Treat(LimbStates& a_states, int a_kind)
    {
        switch (a_kind)
        {
        case kTreatBandage:
        {
            int best = -1;
            for (std::size_t i = 0; i < kLimbCount; ++i)
                if (a_states[i].bleed > 0 && (best < 0 || a_states[i].bleed > a_states[best].bleed))
                    best = static_cast<int>(i);
            if (best >= 0)
            {
                --a_states[best].bleed;
                return true;
            }
            for (std::size_t i = 0; i < kLimbCount; ++i)
                if (a_states[i].trauma == 2 && !a_states[i].splinted)
                {
                    a_states[i].splinted = true;
                    return true;
                }
            return false;
        }
        case kTreatSplint:
        {
            constexpr std::array<std::size_t, 4> order{ kLeftLeg, kRightLeg, kLeftArm, kRightArm };
            for (const auto i : order)
                if (a_states[i].trauma == 3 && !a_states[i].splinted)
                {
                    a_states[i].splinted = true;
                    return true;
                }
            for (const auto i : order)
                if (a_states[i].trauma == 2 && !a_states[i].splinted)
                {
                    a_states[i].splinted = true;
                    return true;
                }
            return false;
        }
        case kTreatTourniquet:
        {
            for (std::size_t i = kLeftArm; i < kLimbCount; ++i)
                if (a_states[i].bleed == 2 && !a_states[i].tourniquet)
                {
                    a_states[i].bleed = 0;
                    a_states[i].tourniquet = true;
                    return true;
                }
            return false;
        }
        case kTreatSutureKit:
        {
            int best = -1;
            int bestKey = 0;
            for (std::size_t i = 0; i < kLimbCount; ++i)
            {
                const auto& s = a_states[i];
                if (s.bleed == 0 && !s.tourniquet)
                    continue;
                const int key = s.bleed * 100 + (s.tourniquet ? 10 : 0) + s.trauma;
                if (best < 0 || key > bestKey)
                {
                    best = static_cast<int>(i);
                    bestKey = key;
                }
            }
            if (best < 0)
                return false;
            a_states[best].bleed = 0;
            a_states[best].tourniquet = false;
            AddProgress(a_states[best], kTreatHeal);
            return true;
        }
        case kTreatMedKit:
        {
            bool any = false;
            for (auto& s : a_states)
            {
                if (s.trauma == 0 && s.bleed == 0 && !s.tourniquet)
                    continue;
                any = true;
                s.bleed = 0;
                s.tourniquet = false;
                if (s.trauma == 1)
                {
                    s.trauma = 0;
                    s.splinted = false;
                    s.heal = 0.0f;
                }
                else if (s.trauma >= 2)
                {
                    s.splinted = true;
                    AddProgress(s, kTreatHeal);
                }
            }
            return any;
        }
        default:
            return false;
        }
    }

    inline void ApplyStimpak(LimbStates& a_states)
    {
        for (auto& s : a_states)
        {
            if (s.trauma == 1)
            {
                s.trauma = 0;
                s.splinted = false;
                s.heal = 0.0f;
            }
            if (s.bleed == 2)
                s.bleed = 1;
            else if (s.bleed == 1)
                s.bleed = 0;
            AddProgress(s, kTreatHeal);
        }
    }

    inline bool HasUnsplintedFracture(const LimbStates& a_states)
    {
        for (const auto& s : a_states)
            if (s.trauma == 3 && !s.splinted)
                return true;
        return false;
    }

    inline bool IsClean(const LimbStates& a_states)
    {
        for (const auto& s : a_states)
            if (s.trauma != 0 || s.bleed != 0 || s.tourniquet)
                return false;
        return true;
    }
}
