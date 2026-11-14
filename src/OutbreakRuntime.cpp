#include "pch.h"
#include "OutbreakRuntime.h"

#include "Hooks.h"
#include "RadHud.h"
#include <RE/A/Actor.h>
#include <RE/E/ExtraDataList.h>
#include <RE/E/ExtraFactionChanges.h>
#include <RE/M/Main.h>
#include <RE/N/NiCamera.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESNPC.h>

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace
{
    constexpr const char* kPlugin = "RadOutbreak.esp";

    constexpr const char* kCompanionPlugin = "Fallout4.esm";

    constexpr std::uint32_t kSickFactionForm = 0x000802;
    constexpr std::uint32_t kScannerForm = 0x000813;
    constexpr std::uint32_t kVanillaScannerForm = 0x154AD3;
    constexpr std::uint32_t kCompanionFactionForm = 0x023C01;

    constexpr float kScannerRange = 4096.0f;
    constexpr float kHeadOffset = 140.0f;
    constexpr float kEdgeMargin = 0.02f;
    constexpr int kLastSickRank = 3;
    constexpr int kImmuneRank = 4;
    constexpr std::size_t kMaxMarkers = 24;
    constexpr std::size_t kMaxCompanions = 8;
    constexpr std::size_t kWidgetCompanions = 3;

    struct Marker
    {
        float distanceSq;
        float x;
        float y;
        int   stage;
    };

    struct Companion
    {
        std::uint32_t id;
        std::string   name;
        int           stage;
    };

    int FactionRank(const RE::Actor* a_actor, const RE::TESFaction* a_faction)
    {
        if (!a_faction)
            return -1;
        if (const auto* list = a_actor->extraList.get())
        {
            if (const auto* changes = list->GetByType<RE::ExtraFactionChanges>())
            {
                for (const auto& entry : changes->factionChanges)
                {
                    if (entry.faction == a_faction)
                        return entry.rank;
                }
            }
        }
        if (const auto* npc = a_actor->GetNPC())
        {
            for (const auto& entry : npc->factions)
            {
                if (entry.faction == a_faction)
                    return entry.rank;
            }
        }
        return -1;
    }

    json CompanionJson(const Companion& a_companion)
    {
        return json{ { "n", a_companion.name }, { "s", a_companion.stage } };
    }

    bool Project(const RE::NiPoint3& a_point, float& a_x, float& a_y)
    {
        const auto* camera = RE::Main::WorldRootCamera();
        if (!camera)
            return false;
        const auto& m = camera->worldToCam;
        const float w = m[3][0] * a_point.x + m[3][1] * a_point.y + m[3][2] * a_point.z + m[3][3];
        if (w <= 0.00001f)
            return false;
        const float x = (m[0][0] * a_point.x + m[0][1] * a_point.y + m[0][2] * a_point.z + m[0][3]) / w;
        const float y = (m[1][0] * a_point.x + m[1][1] * a_point.y + m[1][2] * a_point.z + m[1][3]) / w;
        const auto& port = camera->port;
        const bool  usePort = std::abs(port.right - port.left) > 0.01f && std::abs(port.top - port.bottom) > 0.01f;
        const float width = usePort ? port.right - port.left : 1.0f;
        const float height = usePort ? port.top - port.bottom : 1.0f;
        const float left = usePort ? port.left : 0.0f;
        const float bottom = usePort ? port.bottom : 0.0f;
        a_x = (x * 0.5f + 0.5f) * width + left;
        a_y = 1.0f - ((y * 0.5f + 0.5f) * height + bottom);
        return std::isfinite(a_x) && std::isfinite(a_y);
    }
}

void OutbreakRuntime::Resolve()
{
    if (_faction && _companion && _scanner && _vanillaScanner)
        return;
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data)
        return;
    if (!_faction)
    {
        auto* form = data->LookupForm(kSickFactionForm, kPlugin);
        if (form && form->GetFormType() == RE::ENUM_FORM_ID::kFACT)
            _faction = form;
    }
    if (!_companion)
    {
        auto* form = data->LookupForm(kCompanionFactionForm, kCompanionPlugin);
        if (form && form->GetFormType() == RE::ENUM_FORM_ID::kFACT)
            _companion = form;
    }
    if (!_scanner)
    {
        auto* form = data->LookupForm(kScannerForm, kPlugin);
        if (form && form->GetFormType() == RE::ENUM_FORM_ID::kMISC)
            _scanner = form;
    }
    if (!_vanillaScanner)
    {
        auto* form = data->LookupForm(kVanillaScannerForm, kCompanionPlugin);
        if (form && form->GetFormType() == RE::ENUM_FORM_ID::kMISC)
            _vanillaScanner = form;
    }
}

void OutbreakRuntime::Update()
{
    if (!Hooks::IsGameMainThread())
        return;
    auto& hud = RadHud::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* processLists = RE::ProcessLists::GetSingleton();
    Resolve();
    if (!player || !player->GetParentCell() || !processLists)
    {
        hud.PushOutbreak("[]");
        hud.PushCompanions("[]");
        return;
    }

    const auto* sick = static_cast<const RE::TESFaction*>(_faction);
    const auto* companionFaction = static_cast<const RE::TESFaction*>(_companion);
    const auto  carries = [player](void* item) {
        return item && player->GetInventoryObjectCount(static_cast<const RE::TESBoundObject*>(item)) != 0;
    };
    const bool  markersOn = sick && (carries(_scanner) || carries(_vanillaScanner));
    const auto  origin = player->GetPosition();
    std::vector<Marker>    markers;
    std::vector<Companion> companions;
    int                    nearby[kImmuneRank + 1]{};
    if (sick || companionFaction)
    {
        for (auto& handle : processLists->highActorHandles)
        {
            auto actorPtr = handle.get();
            RE::Actor* actor = actorPtr.get();
            if (!actor || actor->IsPlayerRef() || actor->IsDead(false) || !actor->Get3D())
                continue;
            const int  stage = FactionRank(actor, sick);
            const bool isCompanion = FactionRank(actor, companionFaction) >= 0;
            if (stage < 0 && !isCompanion)
                continue;
            if (isCompanion)
            {
                const char* name = actor->GetDisplayFullName();
                companions.push_back({ actor->formID, name && *name ? name : "Companion", stage > kImmuneRank ? -1 : stage });
            }
            if (stage < 0 || stage > kImmuneRank)
                continue;
            auto head = actor->GetPosition();
            const float dx = head.x - origin.x;
            const float dy = head.y - origin.y;
            const float dz = head.z - origin.z;
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            if (distanceSq > kScannerRange * kScannerRange)
                continue;
            ++nearby[stage];
            if (!markersOn || stage > kLastSickRank)
                continue;
            head.z += kHeadOffset;
            float x = 0.0f;
            float y = 0.0f;
            if (!Project(head, x, y) || x < -kEdgeMargin || x > 1.0f + kEdgeMargin || y < -kEdgeMargin || y > 1.0f + kEdgeMargin)
                continue;
            markers.push_back({ distanceSq, x, y, stage });
        }
    }
    std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) { return a.distanceSq < b.distanceSq; });
    if (markers.size() > kMaxMarkers)
        markers.resize(kMaxMarkers);
    std::sort(companions.begin(), companions.end(), [](const Companion& a, const Companion& b) { return a.id < b.id; });
    if (companions.size() > kMaxCompanions)
        companions.resize(kMaxCompanions);

    if (!markersOn)
    {
        hud.PushOutbreak("[]");
    }
    else
    {
        std::string payload = "[";
        for (std::size_t i = 0; i < markers.size(); ++i)
        {
            char entry[64];
            std::snprintf(entry, sizeof(entry), "%s{\"x\":%.3f,\"y\":%.3f,\"s\":%d}", i ? "," : "",
                markers[i].x, markers[i].y, markers[i].stage);
            payload += entry;
        }
        payload += "]";
        hud.PushOutbreak(payload);
    }

    auto widget = json::array();
    if (sick)
    {
        for (std::size_t i = 0; i < companions.size() && i < kWidgetCompanions; ++i)
            widget.push_back(CompanionJson(companions[i]));
    }
    hud.PushCompanions(widget.dump());

    if (hud.DebugMenuOpen())
    {
        auto list = json::array();
        for (const auto& companion : companions)
            list.push_back(CompanionJson(companion));
        hud.PushOutbreakDebug(json{
            { "sick", sick != nullptr },
            { "companionFaction", companionFaction != nullptr },
            { "scanner", _scanner != nullptr },
            { "you", json{ { "d", hud.YouDisease() }, { "o", hud.YouOnset() } } },
            { "companions", list },
            { "nearby", std::vector<int>(std::begin(nearby), std::end(nearby)) } }.dump());
    }
}

void OutbreakRuntime::Reset()
{
    _faction = nullptr;
    _companion = nullptr;
    _scanner = nullptr;
    _vanillaScanner = nullptr;
}
