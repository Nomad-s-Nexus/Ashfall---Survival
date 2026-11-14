#include "pch.h"
#include "OutbreakQuestHeld.h"

#include <RE/A/Actor.h>
#include <RE/B/BGSRefAliasInstanceData.h>
#include <RE/B/BSScriptUtil.h>
#include <RE/B/BSScript_IVirtualMachine.h>
#include <RE/B/BSSpinLock.h>
#include <RE/E/ExtraAliasInstanceArray.h>
#include <RE/E/ExtraDataList.h>
#include <RE/Q/QuestFlag.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFormUtil.h>
#include <RE/T/TESQuest.h>

#include <cstdint>
#include <optional>
#include <variant>

namespace
{
    constexpr std::uint32_t kFollowersQuest = 0x0289E4;
    constexpr std::uint32_t kWorkshopParentQuest = 0x02058E;
    constexpr std::uint32_t kOutbreakQuest = 0x000803;

    bool IsRunning(const RE::TESQuest* a_quest)
    {
        const auto flags = static_cast<std::uint32_t>(a_quest->data.flags);
        return (flags & static_cast<std::uint32_t>(RE::QuestFlag::kEnabled)) != 0 &&
               (flags & static_cast<std::uint32_t>(RE::QuestFlag::kCompleted)) == 0;
    }

    bool IsQuestHeld(std::monostate, RE::TESForm* a_form)
    {
        auto* a_actor = a_form ? a_form->As<RE::Actor>() : nullptr;
        if (!a_actor || !a_actor->extraList)
            return false;
        auto* array = a_actor->extraList->GetByType<RE::ExtraAliasInstanceArray>();
        if (!array)
            return false;

        const RE::TESForm* outbreak = nullptr;
        if (auto* data = RE::TESDataHandler::GetSingleton())
            outbreak = data->LookupForm(kOutbreakQuest, "RadOutbreak.esp");

        const RE::BSAutoReadLock lock{ array->aliasArrayLock };
        for (const auto& entry : array->aliasArray)
        {
            const auto* quest = entry.quest;
            if (!quest || !IsRunning(quest))
                continue;
            const auto id = quest->GetFormID();
            if (id == kFollowersQuest || id == kWorkshopParentQuest || quest == outbreak)
                continue;
            return true;
        }
        return false;
    }

    bool F4SEAPI RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
    {
        if (!a_vm)
            return false;
        a_vm->BindNativeMethod("RadNatives", "IsQuestHeld", IsQuestHeld, std::nullopt, false);
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->info("[Outbreak] Papyrus bridge registered (RadNatives.IsQuestHeld, global).");
        return true;
    }
}

void OutbreakQuestHeld::Install()
{
    auto* papyrus = F4SE::GetPapyrusInterface();
    if (!papyrus || !papyrus->Register(RegisterPapyrusFunctions))
    {
        if (auto logger = spdlog::get("FalloutAnomaly"))
            logger->warn("[Outbreak] Papyrus bridge unavailable; IsQuestHeld will not resolve and the script call will return None.");
    }
}
