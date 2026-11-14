#include "pch.h"
#include "ClimateEffects.h"

#include "RadNeedsTuning.h"
#include "RadNeedsTuningPolicy.h"
#include "RadiationEngine.h"

#include <RE/A/ACTOR_VALUE_MODIFIER.h>
#include <RE/A/ActorValue.h>
#include <RE/A/ActorValueOwner.h>
#include <RE/B/BSScriptUtil.h>
#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/C/Calendar.h>
#include <RE/D/DifficultyLevel.h>
#include <RE/G/GameScript.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESGlobal.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <optional>
#include <variant>

namespace
{
    constexpr float kHotFoodHeat01 = 0.6f;
    constexpr float kMaxWarmHours = 24.0f;
    constexpr float kMaxDamageStepSeconds = 1.0f;
    constexpr auto kResendInterval = std::chrono::seconds(10);
    constexpr auto kRetryInterval = std::chrono::seconds(2);

    std::atomic<float> g_warmUntilDays{ -1.0f };
    std::atomic<float> g_pendingWarmHours{ 0.0f };
    int g_sentStage{ -1 };
    std::chrono::steady_clock::time_point g_lastSend{};
    std::chrono::steady_clock::time_point g_lastTick{};
    bool g_haveLastTick{ false };

    float GameDays() noexcept
    {
        auto* cal = RE::Calendar::GetSingleton();
        if (cal && cal->gameDaysPassed)
            return cal->gameDaysPassed->GetValue();
        return 0.0f;
    }

    bool EffectsOn() noexcept
    {
        return RadNeedsTuning::On(RadNeedsTuningPolicy::Key::kHypothermiaEffects, true);
    }

    bool DispatchStage(int stage)
    {
        auto* game = RE::GameVM::GetSingleton();
        const auto vm = game ? game->GetVM() : nullptr;
        if (!vm)
            return false;
        const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> noCallback;
        return vm->DispatchStaticCall("RadShowcase", "ApplyCold", noCallback, static_cast<std::int32_t>(stage));
    }

    void SyncStage(int wanted)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto since = now - g_lastSend;
        if (wanted == g_sentStage && since < kResendInterval)
            return;
        if (g_sentStage == -2 && since < kRetryInterval)
            return;
        g_lastSend = now;
        const bool sent = DispatchStage(wanted);
        g_sentStage = sent ? wanted : -2;
        if (!sent)
            return;
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->debug("[ClimateFX] RadShowcase.ApplyCold({})", wanted);
    }

    void ApplyColdDamage(RadClimatePolicy::HypothermiaStage stage, float seconds)
    {
        const float perSecond = RadClimatePolicy::EffectsForStage(stage).healthDamagePerSecond;
        const float scale = RadNeedsTuning::Value(RadNeedsTuningPolicy::Key::kHypothermiaDamage, 1.0f);
        const float amount = perSecond * scale * seconds;
        if (!(amount > 0.0f))
            return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* av = RE::ActorValue::GetSingleton();
        if (!player || !av || !av->health || player->IsDead(false))
            return;
        if (player->GetDifficultyLevel() == RE::DifficultyLevel::kTrueSurvival)
            return;
        const float drain = RadiationEngine::GetSingleton().PlayerHealthDrain(player, av->health, -amount);
        if (drain < 0.0f)
            player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *av->health, drain);
    }

    void ConsumePendingWarmth()
    {
        const float hours = g_pendingWarmHours.exchange(0.0f, std::memory_order_acq_rel);
        if (!(hours > 0.0f))
            return;
        const float until = GameDays() + std::min(hours, kMaxWarmHours) / 24.0f;
        g_warmUntilDays.store(std::max(g_warmUntilDays.load(std::memory_order_acquire), until), std::memory_order_release);
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->info("[ClimateFX] hot food: warmth floor {:.2f} for {:.1f} game hours.", kHotFoodHeat01, hours);
    }

    void PapyrusWarmBody(std::monostate, float a_hours)
    {
        if (!std::isfinite(a_hours) || a_hours <= 0.0f)
            return;
        float current = g_pendingWarmHours.load(std::memory_order_acquire);
        while (!g_pendingWarmHours.compare_exchange_weak(current, std::min(current + a_hours, kMaxWarmHours),
            std::memory_order_acq_rel))
        {
        }
    }

    bool PapyrusIsGodMode(std::monostate)
    {
        return RadiationEngine::GetSingleton().PlayerGodMode(RE::PlayerCharacter::GetSingleton());
    }

    bool F4SEAPI RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
    {
        if (!a_vm)
            return false;
        a_vm->BindNativeMethod("RadNatives", "WarmBody", PapyrusWarmBody, std::nullopt, false);
        a_vm->BindNativeMethod("RadNatives", "IsGodMode", PapyrusIsGodMode, std::nullopt, false);
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->info("[ClimateFX] Papyrus bridge registered (RadNatives.WarmBody, RadNatives.IsGodMode, global).");
        return true;
    }
}

void ClimateEffects::InstallPapyrusBridge()
{
    auto* papyrus = F4SE::GetPapyrusInterface();
    if (!papyrus || !papyrus->Register(RegisterPapyrusFunctions))
    {
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->warn("[ClimateFX] Papyrus bridge unavailable; hot food will not warm the body.");
    }
}

void ClimateEffects::OnTick(RadClimatePolicy::HypothermiaStage stage, bool liveTick)
{
    ConsumePendingWarmth();

    const auto now = std::chrono::steady_clock::now();
    float seconds = 0.0f;
    if (g_haveLastTick)
        seconds = std::chrono::duration<float>(now - g_lastTick).count();
    g_lastTick = now;
    g_haveLastTick = true;

    const bool on = EffectsOn();
    SyncStage(on ? static_cast<int>(stage) : 0);
    if (on && liveTick)
        ApplyColdDamage(stage, std::clamp(seconds, 0.0f, kMaxDamageStepSeconds));
}

float ClimateEffects::WarmthFloor01(float gameDays) noexcept
{
    const float until = g_warmUntilDays.load(std::memory_order_acquire);
    return (until > 0.0f && gameDays < until) ? kHotFoodHeat01 : 0.0f;
}

void ClimateEffects::Resend() noexcept
{
    g_sentStage = -1;
    g_haveLastTick = false;
}

void ClimateEffects::Reset() noexcept
{
    Resend();
    g_warmUntilDays.store(-1.0f, std::memory_order_release);
    g_pendingWarmHours.store(0.0f, std::memory_order_release);
}
