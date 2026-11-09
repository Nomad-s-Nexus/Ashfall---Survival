#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace RadWaterFillPolicy
{
    struct FormRef
    {
        const char*   plugin;
        std::uint32_t localID;
        const char*   editorID;
    };

    inline constexpr std::array<FormRef, 26> kEmptyBottles{ {
        { "Fallout4.esm", 0x0004835A, "NukaColaEmpty" },
        { "Fallout4.esm", 0x000211DD, "BeerBottleEmpty01" },
        { "Fallout4.esm", 0x000DF221, "BottleEmpty01" },
        { "Fallout4.esm", 0x000DF222, "BottleEmpty02" },
        { "Fallout4.esm", 0x000DF223, "BottleEmpty03" },
        { "Fallout4.esm", 0x000DF24D, "BottleEmpty04" },
        { "Fallout4.esm", 0x000DEDDF, "BottleEmptyBourbon01" },
        { "Fallout4.esm", 0x0014A138, "BottleEmptyRum01" },
        { "Fallout4.esm", 0x000DEDE0, "BottleEmptyVodka01" },
        { "Fallout4.esm", 0x000DEDE1, "BottleEmptyWhiskey01" },
        { "Fallout4.esm", 0x000DEDED, "BottleEmptyWine01" },
        { "Fallout4.esm", 0x001BEAEE, "BottleEmptyWineAmontillado" },
        { "Fallout4.esm", 0x0003B55B, "MilkBottleEmpty" },
        { "Fallout4.esm", 0x0011EA9B, "BeerGwinnettBrewEmpty01" },
        { "Fallout4.esm", 0x0011EA9E, "BeerGwinnettAleEmpty01" },
        { "Fallout4.esm", 0x0011EA9C, "BeerGwinnettLagerEmpty01" },
        { "Fallout4.esm", 0x0011EA9D, "BeerGwinnettPaleEmpty01" },
        { "Fallout4.esm", 0x0011EAA0, "BeerGwinnettPilsEmpty01" },
        { "Fallout4.esm", 0x0011EA9F, "BeerGwinnettStoutEmpty01" },
        { "Fallout4.esm", 0x000DEDD9, "PrewarBottleBourbon01" },
        { "Fallout4.esm", 0x000DF031, "PrewarBottleVodka01" },
        { "Fallout4.esm", 0x000DF032, "PrewarBottleWhiskeyEmpty01" },
        { "Fallout4.esm", 0x0017604F, "ChemistrySetBottle01" },
        { "Fallout4.esm", 0x001A89A0, "BabyBottleDirty01" },
        { "Fallout4.esm", 0x001A899B, "BabyBottleDirty02" },
        { "Fallout4.esm", 0x0009B4BA, "DN050BabyBottle" },
    } };

    inline constexpr std::array<FormRef, 10> kWaterSources{ {
        { "Fallout4.esm",      0x000B1CFE, "Vault_WaterFountain01" },
        { "Fallout4.esm",      0x0020DE62, "Vault_WaterFountain01_Rads" },
        { "Fallout4.esm",      0x000C3676, "Vault_Sink_01Activator" },
        { "Fallout4.esm",      0x000F5D8D, "PlayerHouse_KitchenSink01Activator" },
        { "Fallout4.esm",      0x001A4AA4, "PlayerHouse_Ruin_KitchenSink01Activator" },
        { "Fallout4.esm",      0x000DB83B, "WaterPump01Furn" },
        { "DLCCoast.esm",      0x00039A92, "DLC03_Vault_WaterFountain01" },
        { "DLCworkshop03.esm", 0x0000251F, "DLC06Vault_Sink_01Activator" },
        { "DLCworkshop03.esm", 0x00001D3D, "DLC06WaterCooler01FullActivator" },
        { "DLCworkshop03.esm", 0x000049AD, "DLC06WorkshopVault_WaterFountain01" },
    } };

    inline constexpr FormRef kWaterPumpKeyword{ "Fallout4.esm", 0x000DCDD9, "AnimFurnWaterPump" };

    inline constexpr std::array<FormRef, 2> kMaintainedVaultCells{ {
        { "Fallout4.esm", 0x0002A728, "Vault81" },
        { "Fallout4.esm", 0x000A9B62, "Vault81Entry" },
    } };

    inline constexpr FormRef kFilledWater{ "Fallout4.esm", 0x000366BF, "WaterDirty" };

    inline constexpr FormRef kPurifiedWater{ "Fallout4.esm", 0x000366C0, "WaterPurified" };

    inline constexpr std::size_t kBottleNotFound = static_cast<std::size_t>(-1);

    enum class FilledWater
    {
        kDirty,
        kPurified
    };

    enum class Decision
    {
        kFill,
        kDisabled,
        kVanillaSurvival,
        kNotWaterSource,
        kNoModifier,
        kNoEmptyBottle
    };

    constexpr bool IsWaterSource(
        std::uint32_t a_baseFormID,
        bool a_hasWaterPumpKeyword,
        const std::uint32_t* a_sources,
        std::size_t a_sourceCount)
    {
        if (a_hasWaterPumpKeyword)
            return true;
        if (a_baseFormID == 0 || !a_sources)
            return false;
        for (std::size_t i = 0; i < a_sourceCount; ++i)
            if (a_sources[i] == a_baseFormID)
                return true;
        return false;
    }

    constexpr bool IsMaintainedVaultCell(
        std::uint32_t a_cellFormID,
        const std::uint32_t* a_cells,
        std::size_t a_cellCount)
    {
        if (a_cellFormID == 0 || !a_cells)
            return false;
        for (std::size_t i = 0; i < a_cellCount; ++i)
            if (a_cells[i] == a_cellFormID)
                return true;
        return false;
    }

    constexpr FilledWater FilledWaterFor(bool a_inMaintainedVault)
    {
        return a_inMaintainedVault ? FilledWater::kPurified : FilledWater::kDirty;
    }

    constexpr std::size_t FirstAvailableBottle(const std::uint32_t* a_counts, std::size_t a_count)
    {
        if (!a_counts)
            return kBottleNotFound;
        for (std::size_t i = 0; i < a_count; ++i)
            if (a_counts[i] > 0)
                return i;
        return kBottleNotFound;
    }

    constexpr Decision Decide(
        bool a_enabled,
        bool a_waterSource,
        bool a_vanillaSurvival,
        bool a_modifierHeld,
        std::size_t a_availableBottles)
    {
        if (!a_enabled)
            return Decision::kDisabled;
        if (!a_waterSource)
            return Decision::kNotWaterSource;
        if (a_vanillaSurvival)
            return Decision::kVanillaSurvival;
        if (!a_modifierHeld)
            return Decision::kNoModifier;
        if (a_availableBottles == 0)
            return Decision::kNoEmptyBottle;
        return Decision::kFill;
    }
}
