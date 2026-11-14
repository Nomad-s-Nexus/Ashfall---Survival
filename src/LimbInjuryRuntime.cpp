#include "pch.h"
#include "LimbInjuryRuntime.h"

#include "Hooks.h"
#include "RadNeedsTuning.h"
#include <RE/A/ACTOR_VALUE_MODIFIER.h>
#include <RE/A/ActorState.h>
#include <RE/A/ActorValue.h>
#include <RE/A/ActorValueInfo.h>
#include <RE/A/AlchemyItem.h>
#include <RE/B/BSTEvent.h>
#include <RE/C/Calendar.h>
#include <RE/D/DialogueMenu.h>
#include <RE/H/HitData.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESEquipEvent.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TESHitEvent.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/U/UI.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>

using namespace LimbInjuryPolicy;

namespace
{
    using Key = RadNeedsTuningPolicy::Key;

    constexpr const char* kPlugin = "RadInjury.esp";
    constexpr std::uint32_t kLimbGlobalBase = 0x000800;
    constexpr std::uint32_t kFxGlobalBase = 0x000806;
    constexpr std::uint32_t kItemBase = 0x00081E;
    constexpr std::uint32_t kStimpakForm = 0x023736;
    constexpr std::uint32_t kPlayerRefID = 0x14;

    constexpr double kHitWindowSec = 1.0;
    constexpr double kAirWindowSec = 2.0;
    constexpr double kPairWindowSec = 2.0;
    constexpr float  kDropEpsilon = 0.5f;
    constexpr float  kZeroEpsilon = 0.01f;
    constexpr float  kExternalRestore = 10.0f;
    constexpr float  kDoctorLimbRise = 50.0f;
    constexpr float  kDoctorHealthFraction = 0.30f;
    constexpr float  kLightBleedPerSec = 0.2f;
    constexpr float  kHeavyBleedPerSec = 1.0f;
    constexpr float  kRestHours = 0.25f;
    constexpr float  kMaxDtReal = 1.0f;

    constexpr const char* kStimNotice = "A stimpak can't set a fracture. Splint it.";

    RE::TESGlobal* ResolveGlobal(RE::TESDataHandler* a_data, std::uint32_t a_formID)
    {
        RE::TESForm* form = a_data->LookupForm(a_formID, kPlugin);
        return (form && form->GetFormType() == RE::ENUM_FORM_ID::kGLOB)
            ? static_cast<RE::TESGlobal*>(form) : nullptr;
    }

    float ReadGlobal(const RE::TESGlobal* a_global)
    {
        if (!a_global || !std::isfinite(a_global->value))
            return 0.0f;
        return a_global->value;
    }

    RE::ActorValueInfo* ConditionInfo(const RE::ActorValue* a_av, std::size_t a_limb)
    {
        switch (a_limb)
        {
        case kHead: return a_av->perceptionCondition;
        case kTorso: return a_av->enduranceCondition;
        case kLeftArm: return a_av->leftAttackCondition;
        case kRightArm: return a_av->rightAttackCondition;
        case kLeftLeg: return a_av->leftMobiltyCondition;
        case kRightLeg: return a_av->rightMobilityCondition;
        default: return nullptr;
        }
    }

    void Notify(const char* a_message, bool a_warning)
    {
        RE::SendHUDMessage::ShowHUDMessage(a_message, "", true, a_warning);
    }

    const char* FailMessage(int a_kind)
    {
        switch (a_kind)
        {
        case kTreatBandage: return "Nothing to bandage.";
        case kTreatSplint: return "Nothing to splint.";
        case kTreatTourniquet: return "No heavy bleeding on a limb to tie off.";
        case kTreatSutureKit: return "Nothing to suture.";
        case kTreatMedKit: return "No injuries to treat.";
        default: return "Nothing to treat.";
        }
    }

    int ParseLimb(const std::string& a_text)
    {
        static constexpr std::pair<std::string_view, int> names[] = {
            { "head", kHead }, { "torso", kTorso }, { "larm", kLeftArm }, { "leftarm", kLeftArm },
            { "rarm", kRightArm }, { "rightarm", kRightArm }, { "lleg", kLeftLeg }, { "leftleg", kLeftLeg },
            { "rleg", kRightLeg }, { "rightleg", kRightLeg } };
        for (const auto& [name, index] : names)
            if (a_text == name)
                return index;
        if (a_text.size() == 1 && a_text[0] >= '0' && a_text[0] < static_cast<char>('0' + kLimbCount))
            return a_text[0] - '0';
        return -1;
    }

    std::vector<std::string> Split(const std::string& a_text)
    {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (true)
        {
            const auto at = a_text.find(':', start);
            parts.push_back(a_text.substr(start, at == std::string::npos ? at : at - start));
            if (at == std::string::npos)
                break;
            start = at + 1;
        }
        return parts;
    }

    class HitSink final : public RE::BSTEventSink<RE::TESHitEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
        {
            if (!a_event.target || !a_event.target->IsPlayerRef())
                return RE::BSEventNotifyControl::kContinue;
            auto cause = HitCause::Ballistic;
            if (a_event.usesHitData)
            {
                if (a_event.hitData.flags.any(RE::HitData::Flag::kExplosion))
                    cause = HitCause::Explosion;
                else if (a_event.hitData.flags.any(RE::HitData::Flag::kMeleeAttack) ||
                         a_event.hitData.flags.any(RE::HitData::Flag::kBash))
                    cause = HitCause::Melee;
            }
            LimbInjuryRuntime::GetSingleton().OnPlayerHit(cause);
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class EquipSink final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
        {
            if (a_event.equipped && a_event.actor && a_event.actor->IsPlayerRef())
                LimbInjuryRuntime::GetSingleton().OnItemEquipped(a_event.baseObject);
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class ContainerSink final : public RE::BSTEventSink<RE::TESContainerChangedEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent& a_event,
            RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
        {
            if (a_event.oldContainerFormID == kPlayerRefID && a_event.newContainerFormID == 0)
                LimbInjuryRuntime::GetSingleton().OnItemRemoved(a_event.baseObjectFormID);
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

double LimbInjuryRuntime::NowSeconds()
{
    static const auto epoch = Clock::now();
    return std::chrono::duration<double>(Clock::now() - epoch).count();
}

void LimbInjuryRuntime::EnsureSinks()
{
    if (_sinksRegistered)
        return;
    auto* hit = RE::TESHitEvent::GetEventSource();
    auto* equip = RE::TESEquipEvent::GetEventSource();
    auto* container = RE::TESContainerChangedEvent::GetEventSource();
    if (!hit || !equip || !container)
        return;
    static HitSink hitSink;
    static EquipSink equipSink;
    static ContainerSink containerSink;
    hit->RegisterSink(&hitSink);
    equip->RegisterSink(&equipSink);
    container->RegisterSink(&containerSink);
    _sinksRegistered = true;
    if (auto logger = spdlog::get("FalloutAnomaly"))
        logger->info("[Limb] Hit, equip and container sinks registered.");
}

void LimbInjuryRuntime::ResolveForms()
{
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data)
        return;
    for (std::size_t i = 0; i < _limbGlobals.size(); ++i)
        if (!_limbGlobals[i])
            _limbGlobals[i] = ResolveGlobal(data, kLimbGlobalBase + static_cast<std::uint32_t>(i));
    for (std::size_t i = 0; i < _fxGlobals.size(); ++i)
        if (!_fxGlobals[i])
            _fxGlobals[i] = ResolveGlobal(data, kFxGlobalBase + static_cast<std::uint32_t>(i));
    if (_formsResolved)
        return;
    std::lock_guard lock(_eventMutex);
    for (std::size_t i = 0; i < _itemIds.size(); ++i)
    {
        auto* form = data->LookupForm(kItemBase + static_cast<std::uint32_t>(i), kPlugin);
        _itemIds[i] = (form && form->GetFormType() == RE::ENUM_FORM_ID::kALCH) ? form->GetFormID() : 0;
    }
    auto* stimpak = _itemIds[0] ? data->LookupForm(kStimpakForm, "Fallout4.esm") : nullptr;
    _stimpakId = (stimpak && stimpak->GetFormType() == RE::ENUM_FORM_ID::kALCH) ? stimpak->GetFormID() : 0;
    _formsResolved = true;
}

void LimbInjuryRuntime::LoadFromGlobals()
{
    for (std::size_t i = 0; i < kLimbCount; ++i)
        _states[i] = Decode(ReadGlobal(_limbGlobals[i]));
    _loaded = true;
}

void LimbInjuryRuntime::Commit(bool a_enabled)
{
    RegionLevelSet fx;
    if (a_enabled)
        fx = RegionLevels(_states);
    const std::array<float, 4> fxValues{ static_cast<float>(fx.legs), static_cast<float>(fx.arms),
        static_cast<float>(fx.head), static_cast<float>(fx.torso) };
    for (std::size_t i = 0; i < _fxGlobals.size(); ++i)
        if (_fxGlobals[i])
            _fxGlobals[i]->value = fxValues[i];
    for (std::size_t i = 0; i < kLimbCount; ++i)
        if (_limbGlobals[i])
            _limbGlobals[i]->value = Encode(_states[i]);

    std::lock_guard lock(_hudMutex);
    _hud.states = a_enabled ? _states : LimbStates{};
    _hud.condition = _prev;
    _hud.fx = fx;
}

LimbInjuryRuntime::HudSnapshot LimbInjuryRuntime::GetHudSnapshot() const
{
    std::lock_guard lock(_hudMutex);
    return _hud;
}

void LimbInjuryRuntime::ReadConditions(RE::PlayerCharacter* a_player, std::array<float, kLimbCount>& a_out) const
{
    const auto* av = RE::ActorValue::GetSingleton();
    for (std::size_t i = 0; i < kLimbCount; ++i)
    {
        const auto* info = ConditionInfo(av, i);
        a_out[i] = info ? a_player->GetActorValue(*info) : 0.0f;
    }
}

void LimbInjuryRuntime::OnPlayerHit(HitCause a_cause)
{
    const double now = NowSeconds();
    const bool   keepExplosion = static_cast<HitCause>(_lastHitCause.load()) == HitCause::Explosion &&
                                 now - _lastHitSec.load() <= kHitWindowSec;
    if (!keepExplosion || a_cause == HitCause::Explosion)
        _lastHitCause.store(static_cast<int>(a_cause));
    _lastHitSec.store(now);
}

void LimbInjuryRuntime::OnItemEquipped(std::uint32_t a_formID)
{
    std::lock_guard lock(_eventMutex);
    if (a_formID == 0 || (a_formID != _stimpakId && std::find(_itemIds.begin(), _itemIds.end(), a_formID) == _itemIds.end()))
        return;
    const double now = NowSeconds();
    std::erase_if(_removed, [&](const auto& e) { return now - e.second > kPairWindowSec; });
    const auto match = std::find_if(_removed.begin(), _removed.end(), [&](const auto& e) { return e.first == a_formID; });
    if (match != _removed.end())
    {
        _removed.erase(match);
        _usedItems.push_back(a_formID);
        return;
    }
    std::erase_if(_equipped, [&](const auto& e) { return now - e.second > kPairWindowSec; });
    _equipped.emplace_back(a_formID, now);
}

void LimbInjuryRuntime::OnItemRemoved(std::uint32_t a_formID)
{
    std::lock_guard lock(_eventMutex);
    if (a_formID == 0 || (a_formID != _stimpakId && std::find(_itemIds.begin(), _itemIds.end(), a_formID) == _itemIds.end()))
        return;
    const double now = NowSeconds();
    std::erase_if(_equipped, [&](const auto& e) { return now - e.second > kPairWindowSec; });
    const auto match = std::find_if(_equipped.begin(), _equipped.end(), [&](const auto& e) { return e.first == a_formID; });
    if (match != _equipped.end())
    {
        _equipped.erase(match);
        _usedItems.push_back(a_formID);
        return;
    }
    std::erase_if(_removed, [&](const auto& e) { return now - e.second > kPairWindowSec; });
    _removed.emplace_back(a_formID, now);
}

void LimbInjuryRuntime::AfterStimpak(RE::PlayerCharacter* a_player, bool a_crippleOn)
{
    ApplyStimpak(_states);
    if (!a_crippleOn || !HasUnsplintedFracture(_states))
        return;
    HoldCripple(a_player, true);
    if (!_stimNoticeShown)
    {
        _stimNoticeShown = true;
        Notify(kStimNotice, true);
    }
}

void LimbInjuryRuntime::ProcessItems(RE::PlayerCharacter* a_player, bool a_crippleOn)
{
    std::vector<std::uint32_t> used;
    std::array<std::uint32_t, 5> ids{};
    std::uint32_t stimpak = 0;
    {
        std::lock_guard lock(_eventMutex);
        used.swap(_usedItems);
        ids = _itemIds;
        stimpak = _stimpakId;
    }
    for (const auto formID : used)
    {
        if (formID == stimpak)
        {
            AfterStimpak(a_player, a_crippleOn);
            continue;
        }
        const auto at = std::find(ids.begin(), ids.end(), formID);
        if (at == ids.end())
            continue;
        const int kind = static_cast<int>(at - ids.begin()) + 1;
        if (Treat(_states, kind))
            continue;
        auto* form = RE::TESForm::GetFormByID(formID);
        if (form && form->GetFormType() == RE::ENUM_FORM_ID::kALCH)
            a_player->AddObjectToContainer(static_cast<RE::AlchemyItem*>(form), nullptr, 1, nullptr, RE::ITEM_REMOVE_REASON::kNone);
        Notify(FailMessage(kind), true);
    }
}

void LimbInjuryRuntime::ApplyDrops(const std::array<float, kLimbCount>& a_current, double a_now)
{
    const bool  recentHit = a_now - _lastHitSec.load() <= kHitWindowSec;
    const auto  hitCause = static_cast<HitCause>(_lastHitCause.load());
    const bool  airborne = !_airSeen || a_now - _lastAirSec <= kAirWindowSec;
    const float scale = RadNeedsTuning::Value(Key::kLimbInjuryScale, 1.0f);
    for (std::size_t i = 0; i < kLimbCount; ++i)
    {
        const float drop = _prev[i] - a_current[i];
        if (!(drop >= kDropEpsilon))
            continue;
        HitCause cause = HitCause::Unknown;
        if (recentHit)
            cause = hitCause;
        else if (IsLeg(i) && airborne)
            cause = HitCause::Fall;
        const bool crippled = a_current[i] <= kZeroEpsilon;
        _states[i] = ApplyHit(_states[i], ClassifyHit(drop * scale, cause, crippled, i));
    }
}

void LimbInjuryRuntime::ApplyBleed(RE::PlayerCharacter* a_player, float a_dtReal)
{
    const float rate = RadNeedsTuning::Value(Key::kLimbBleedRate, 1.0f);
    float       damage = 0.0f;
    for (const auto& s : _states)
    {
        if (s.tourniquet)
            continue;
        damage += s.bleed == 2 ? kHeavyBleedPerSec : (s.bleed == 1 ? kLightBleedPerSec : 0.0f);
    }
    damage *= rate * a_dtReal;
    if (!(damage > 0.0f) || a_player->IsDead(false))
        return;
    const auto* av = RE::ActorValue::GetSingleton();
    const float health = a_player->GetActorValue(*av->health);
    if (health <= 1.0f)
        return;
    a_player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *av->health, -std::min(damage, health - 1.0f));
}

void LimbInjuryRuntime::HoldCripple(RE::PlayerCharacter* a_player, bool a_enabled)
{
    const auto* av = RE::ActorValue::GetSingleton();
    for (std::size_t i = 0; i < kLimbCount; ++i)
    {
        auto* info = ConditionInfo(av, i);
        if (!info)
            continue;
        const bool want = a_enabled && _states[i].trauma == 3 && !_states[i].splinted;
        if (want)
        {
            const float current = a_player->GetActorValue(*info);
            if (current > kZeroEpsilon)
            {
                a_player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *info, -current);
                _held[i] += current;
            }
            else if (_held[i] <= 0.0f)
                _held[i] = std::max(0.0f, a_player->GetPermanentActorValue(*info) - current);
        }
        else if (_held[i] > 0.0f)
            ReleaseHeld(a_player, i);
    }
}

void LimbInjuryRuntime::ReleaseHeld(RE::PlayerCharacter* a_player, std::size_t a_limb)
{
    const auto* av = RE::ActorValue::GetSingleton();
    auto* info = av ? ConditionInfo(av, a_limb) : nullptr;
    if (info)
    {
        const float room = std::max(0.0f, a_player->GetPermanentActorValue(*info) - a_player->GetActorValue(*info));
        const float give = std::min(_held[a_limb], room);
        if (give > 0.0f)
            a_player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *info, give);
    }
    _held[a_limb] = 0.0f;
}

bool LimbInjuryRuntime::CheckDoctor(RE::PlayerCharacter* a_player, const std::array<float, kLimbCount>& a_current, float a_health)
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui || !ui->GetMenuOpen<RE::DialogueMenu>())
        return false;
    float limbRise = 0.0f;
    for (std::size_t i = 0; i < kLimbCount; ++i)
        limbRise += std::max(0.0f, a_current[i] - _prev[i]);
    const auto* av = RE::ActorValue::GetSingleton();
    const float maxHealth = a_player->GetPermanentActorValue(*av->health);
    const float healthRise = a_health - _prevHealth;
    return limbRise >= kDoctorLimbRise || (maxHealth > 0.0f && healthRise >= kDoctorHealthFraction * maxHealth);
}

void LimbInjuryRuntime::Update()
{
    if (!Hooks::IsGameMainThread())
        return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->GetParentCell())
        return;

    std::lock_guard lock(_mutex);
    EnsureSinks();
    ResolveForms();
    for (const auto* global : _limbGlobals)
        if (!global)
            return;
    const auto* av = RE::ActorValue::GetSingleton();
    if (!av || !av->health)
        return;
    for (std::size_t i = 0; i < kLimbCount; ++i)
        if (!ConditionInfo(av, i))
            return;
    if (!_loaded)
        LoadFromGlobals();

    const bool enabled = RadNeedsTuning::On(Key::kLimbInjuriesEnabled, true);
    if (!enabled)
    {
        HoldCripple(player, false);
        _havePrev = false;
        _haveHours = false;
        {
            std::lock_guard events(_eventMutex);
            _usedItems.clear();
        }
        ReadConditions(player, _prev);
        Commit(false);
        return;
    }

    const double now = NowSeconds();
    const float  dtReal = _lastReal > 0.0 ? static_cast<float>(std::clamp(now - _lastReal, 0.0, static_cast<double>(kMaxDtReal))) : 0.0f;
    _lastReal = now;
    const bool crippleOn = RadNeedsTuning::On(Key::kLimbCrippleFractures, true);

    std::array<float, kLimbCount> current{};
    ReadConditions(player, current);
    const float health = player->GetActorValue(*av->health);

    float dtHours = 0.0f;
    if (auto* calendar = RE::Calendar::GetSingleton())
    {
        const float hours = calendar->GetHoursPassed();
        if (_haveHours && hours >= _lastHours)
            dtHours = hours - _lastHours;
        _lastHours = hours;
        _haveHours = true;
    }

    const auto state = player->DoGetCharacterState();
    if (state == RE::IMovementState::CHARACTER_STATE::kInAir)
    {
        _lastAirSec = now;
        _airSeen = true;
    }

    if (!_havePrev)
    {
        _prev = current;
        _prevHealth = health;
        _havePrev = true;
    }
    else if (CheckDoctor(player, current, health))
    {
        for (std::size_t i = 0; i < kLimbCount; ++i)
            if (_held[i] > 0.0f)
                ReleaseHeld(player, i);
        _states = {};
        _clot = {};
        _held = {};
        _prev = current;
        _prevHealth = health;
        Notify("The doctor treated your injuries.", false);
    }
    else
    {
        for (std::size_t i = 0; i < kLimbCount; ++i)
            if (_held[i] > 0.0f && current[i] - _prev[i] >= kExternalRestore)
                _held[i] = 0.0f;
        ProcessItems(player, crippleOn);
        ApplyDrops(current, now);
        for (std::size_t i = 0; i < kLimbCount; ++i)
            LimbInjuryPolicy::Heal(_states[i], dtHours, dtHours > kRestHours,
                RadNeedsTuning::Value(Key::kLimbHealRate, 1.0f), dtReal, _clot[i]);
        ApplyBleed(player, dtReal);
    }

    if (!HasUnsplintedFracture(_states))
        _stimNoticeShown = false;
    HoldCripple(player, crippleOn);
    ReadConditions(player, _prev);
    _prevHealth = player->GetActorValue(*av->health);
    Commit(true);
}

void LimbInjuryRuntime::Reset()
{
    std::lock_guard lock(_mutex);
    _limbGlobals = {};
    _fxGlobals = {};
    _formsResolved = false;
    _loaded = false;
    _havePrev = false;
    _haveHours = false;
    _stimNoticeShown = false;
    _states = {};
    _clot = {};
    _held = {};
    _prev = {};
    _prevHealth = 0.0f;
    _lastReal = 0.0;
    _lastAirSec = -1.0e9;
    _airSeen = false;
    _lastHitSec.store(-1.0e9);
    {
        std::lock_guard events(_eventMutex);
        _equipped.clear();
        _removed.clear();
        _usedItems.clear();
        _itemIds = {};
        _stimpakId = 0;
    }
    std::lock_guard hud(_hudMutex);
    _hud = {};
}

void LimbInjuryRuntime::HandleDebug(const std::string& a_command)
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player)
        return;
    std::lock_guard lock(_mutex);
    ResolveForms();
    for (const auto* global : _limbGlobals)
        if (!global)
            return;
    if (!_loaded)
        LoadFromGlobals();

    const auto parts = Split(a_command);
    const auto& verb = parts[0];
    const bool  crippleOn = RadNeedsTuning::On(Key::kLimbCrippleFractures, true);
    const auto  number = [&](std::size_t a_index) {
        return a_index < parts.size() ? static_cast<float>(std::atof(parts[a_index].c_str())) : 0.0f;
    };
    const int limb = parts.size() > 1 ? ParseLimb(parts[1]) : -1;

    if (verb == "injure" && limb >= 0)
    {
        LimbState next;
        next.trauma = std::clamp(static_cast<int>(number(2)), 0, 3);
        _states[static_cast<std::size_t>(limb)] = next;
    }
    else if (verb == "bleed" && limb >= 0)
    {
        auto& s = _states[static_cast<std::size_t>(limb)];
        s.bleed = std::clamp(static_cast<int>(number(2)), 0, 2);
        s.tourniquet = false;
        _clot[static_cast<std::size_t>(limb)] = 0.0f;
    }
    else if (verb == "treat")
    {
        const int kind = static_cast<int>(number(1));
        if (!Treat(_states, kind))
            Notify(FailMessage(kind), true);
    }
    else if (verb == "stim")
    {
        AfterStimpak(player, crippleOn);
    }
    else if (verb == "heal")
    {
        for (std::size_t i = 0; i < kLimbCount; ++i)
            LimbInjuryPolicy::Heal(_states[i], number(1), false, 1.0f, 0.0f, _clot[i]);
    }
    else if (verb == "clear")
    {
        _states = {};
        _clot = {};
    }
    else
    {
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->warn("[Limb] Unknown debug command '{}'.", a_command);
        return;
    }
    Commit(RadNeedsTuning::On(Key::kLimbInjuriesEnabled, true));
}
