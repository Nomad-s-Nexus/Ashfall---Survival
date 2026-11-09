#include "RadCorrectnessPolicy.h"
#include "RadWaterFillPolicy.h"
#include "RadReliefPolicy.h"
#include "ClimatePolicy.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <cstring>
#include <limits>

namespace
{
    void Require(bool condition)
    {
        if (!condition)
            std::abort();
    }

    bool Near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    RadClimatePolicy::ClimateState AdvanceClimate(
        RadClimatePolicy::ClimateState state,
        const RadClimatePolicy::EnvironmentSample& environment,
        const RadClimatePolicy::ProtectionSample& protection,
        int seconds)
    {
        for (int i = 0; i < seconds; ++i)
            state = RadClimatePolicy::Step(state, environment, protection, 1.0f).state;
        return state;
    }
}

int main()
{
    using namespace RadCorrectnessPolicy;
    using namespace RadWaterFillPolicy;

    const auto nuke = DistantNukePlacementXY({ 123.0f, 456.0f, 789.0f }, 0.0f, 4250.0f);
    Require(nuke.strike.x == 123.0f);
    Require(nuke.strike.y == 4706.0f);
    Require(std::abs(nuke.facingYaw - 3.14159265f) < 0.001f);
    Require(ShouldReturnToClear(true, false));
    Require(ShouldReturnToClear(false, true));
    Require(!ShouldReturnToClear(false, false));
    Require(UsesVanillaRadstormSky(0));
    Require(!UsesVanillaRadstormSky(1));
    Require(UsesVanillaRadstormSky(2));
    Require(!UsesVanillaRadstormSky(3));
    Require(!UsesVanillaRadstormSky(4));
    Require(!UsesVanillaRadstormSky(-1));
    Require(!UsesVanillaRadstormSky(5));
    Require(StormShakeStrength(0.75f, false) == 0.75f);
    Require(std::abs(StormShakeStrength(0.75f, true) - 0.2625f) < 1e-6f);
    Require(IsGasMask(true, true));
    Require(!IsGasMask(true, false));
    Require(IsConfiguredMask(false, true, true));
    Require(IsConfiguredMask(true, true, true));
    Require(IsConfiguredMask(true, false, false));
    Require(IsConfiguredMask(true, true, false));
    Require(!IsConfiguredMask(false, true, false));
    Require(!IsConfiguredMask(false, false, true));
    Require(!IsConfiguredMask(false, false, false));
    Require(IsRadShieldedMask(true, true));
    Require(!IsRadShieldedMask(true, false));
    Require(!IsRadShieldedMask(false, true));
    Require(MaskOverlayVisible(-1, true, false, true));
    Require(!MaskOverlayVisible(-1, true, true, true));
    Require(!MaskOverlayVisible(-1, false, false, true));
    Require(!MaskOverlayVisible(-1, false, true, true));
    Require(MaskOverlayVisible(1, false, true, true));
    Require(!MaskOverlayVisible(-1, true, false, false));
    Require(MaskOverlayVisible(1, false, false, false));
    Require(!MaskOverlayVisible(0, true, false, true));
    Require(IsUsableHotkey(0));
    Require(IsUsableHotkey(117));
    Require(IsUsableHotkey(0x76));
    Require(!IsUsableHotkey(0x77));
    Require(!IsUsableHotkey(0xFF + 1));
    Require(!IsUsableHotkey(256));
    Require(!IsUsableHotkey(-1));
    Require(!IsUsableHotkey(1));
    Require(!IsUsableHotkey(6));
    Require(!IsUsableHotkey(0x10));
    Require(!IsUsableHotkey(0x1B));
    int hotkey = -1;
    Require(ParseHotkey("117", hotkey) && hotkey == 117);
    Require(ParseHotkey("0x75", hotkey) && hotkey == 0x75);
    Require(ParseHotkey(" 0XF6 ", hotkey) && hotkey == 0xF6);
    Require(!ParseHotkey("", hotkey));
    Require(!ParseHotkey("-1", hotkey));
    Require(!ParseHotkey("zz", hotkey));
    Require(!ParseHotkey("0x1000", hotkey));
    Require(!ParseHotkey("010", hotkey));
    Require(!ParseHotkey("08", hotkey));
    Require(!ParseHotkey("+8", hotkey));
    Require(!ParseHotkey("+010", hotkey));
    Require((ParseHotkey("0", hotkey) && hotkey == 0));
    Require(ParseHotkey("117 ; F6", hotkey) && hotkey == 117);
    Require(ParseHotkey("0x75;F6", hotkey) && hotkey == 0x75);
    Require(!ParseHotkey("; only a comment", hotkey));
    Require(!IsUsableHotkey(0xA0) && !IsUsableHotkey(0xA1));
    static_assert(kChokeGraceTicks > kAutoMaskStableTicks);
    const auto dupPair = ValidateHotkeyPair(0x75, 0x75);
    Require(dupPair.replace == 0 && dupPair.repair == 0 && dupPair.duplicateRejected);
    const auto reservedPair = ValidateHotkeyPair(0x77, 118);
    Require(reservedPair.replace == 0 && reservedPair.repair == 118 && reservedPair.replaceRejected);
    const auto goodPair = ValidateHotkeyPair(0x75, 0x76);
    Require(goodPair.replace == 0x75 && goodPair.repair == 0x76 && !goodPair.duplicateRejected);
    RadReliefPolicy::FormRef reliefRef{};
    Require(RadReliefPolicy::ParseFormRef("Fallout4.esm:0x058AB8", reliefRef));
    Require(reliefRef.plugin == "Fallout4.esm" && reliefRef.localID == 0x058AB8);
    Require(RadReliefPolicy::ParseFormRef("  Some Mod.esp : 0Xabc ", reliefRef));
    Require(reliefRef.plugin == "Some Mod.esp" && reliefRef.localID == 0xABC);
    Require(RadReliefPolicy::ParseFormRef("Fallout4.esm:1678008", reliefRef));
    Require(reliefRef.localID == 1678008);
    Require(!RadReliefPolicy::ParseFormRef("Fallout4.esm", reliefRef));
    Require(!RadReliefPolicy::ParseFormRef("Fallout4.esm:0x1000000", reliefRef));
    Require(!RadReliefPolicy::ParseFormRef("Fallout4.esm:0", reliefRef));
    Require(!RadReliefPolicy::ParseFormRef(":0x800", reliefRef));
    Require(!RadReliefPolicy::ParseFormRef("Fallout4.esm:0xZZ", reliefRef));
    Require(!RadReliefPolicy::ParseFormRef("Fallout4.esm:010", reliefRef));
    Require(RadReliefPolicy::ParseFormRef("Fallout4.esm:0x10", reliefRef) && reliefRef.localID == 0x10);
    Require(RadReliefPolicy::ParseFormRef("Fallout4.esm:16", reliefRef) && reliefRef.localID == 16);
    std::size_t reliefInvalid = 0;
    const auto  reliefList = RadReliefPolicy::ParseList(
        " Fallout4.esm:0x58AB8 , , Fallout4.esm:0x0E9284, broken ", &reliefInvalid);
    Require(reliefList.size() == 2);
    Require(reliefInvalid == 1);
    Require(reliefList[0].localID == 0x58AB8 && reliefList[1].localID == 0xE9284);
    Require(RadReliefPolicy::ParseList("", nullptr).empty());
    Require(RadReliefPolicy::SplitEntries(" A:0x1 , , B.esp:2, ").size() == 2);
    {
        const auto tokens = RadReliefPolicy::SplitEntries("A.esp:0x1, B.esp:0x2 ; food, drink");
        Require(tokens.size() == 2);
        Require(tokens[0] == "A.esp:0x1");
        Require(tokens[1] == "B.esp:0x2");
    }
    Require(RadReliefPolicy::ParseList("A.esp:0x1 ; note").size() == 1);
    Require(RadWeatherExposed(true, false, false, false, false));
    Require(!RadWeatherExposed(true, true, false, false, false));
    Require(!RadWeatherExposed(true, false, true, false, false));
    Require(!RadWeatherExposed(true, false, false, true, false));
    Require(!RadWeatherExposed(true, false, false, false, true));
    Require(!RadWeatherExposed(false, false, false, false, false));
    const auto synced = StepAutoMask(-1, -1, 0, true, 3);
    Require(!synced.equip && !synced.remove && synced.applied == 1 && synced.streak == 0);
    const auto firstSample = StepAutoMask(0, 0, 0, true, 3);
    Require(!firstSample.equip && firstSample.candidate == 1 && firstSample.streak == 1);
    const auto secondSample = StepAutoMask(0, firstSample.candidate, firstSample.streak, true, 3);
    Require(!secondSample.equip && secondSample.streak == 2);
    const auto stableEquip = StepAutoMask(0, secondSample.candidate, secondSample.streak, true, 3);
    Require(stableEquip.equip && !stableEquip.remove && stableEquip.applied == 1);
    const auto flap = StepAutoMask(1, 1, 2, false, 3);
    Require(!flap.remove && flap.candidate == 0 && flap.streak == 1);
    const auto stableRemove = StepAutoMask(1, 0, 2, false, 3);
    Require(stableRemove.remove && !stableRemove.equip && stableRemove.applied == 0);
    Require(!CuresPermanentTissue(CureSignal::kNone));
    Require(!CuresPermanentTissue(CureSignal::kInventoryMovement));
    Require(CuresPermanentTissue(CureSignal::kVerifiedChelationUse));
    Require(!CuresPermanentTissue(CureSignal::kVerifiedRadAwayUse));
    Require(!ArmsRadAwayHealing(CureSignal::kNone));
    Require(!ArmsRadAwayHealing(CureSignal::kInventoryMovement));
    Require(!ArmsRadAwayHealing(CureSignal::kVerifiedChelationUse));
    Require(ArmsRadAwayHealing(CureSignal::kVerifiedRadAwayUse));

    Require(!IsEngineActive(false, false));
    Require(!IsEngineActive(true, false));
    Require(!IsEngineActive(false, true));
    Require(IsEngineActive(true, true));

    Require(ShouldStartHudWait(false, 42, 42));
    Require(!ShouldStartHudWait(true, 42, 42));
    Require(!ShouldStartHudWait(false, 42, 43));
    Require(IsCurrentLoadGeneration(42, 42));
    Require(!IsCurrentLoadGeneration(42, 43));

    std::atomic<std::uint64_t> newGameGeneration{ 41 };
    std::atomic<bool> newGameHudWaiting{ true };
    Require(BeginNewGameLoad(newGameGeneration, newGameHudWaiting) == 42);
    Require(!newGameHudWaiting.load());

    using namespace RadClimatePolicy;

    Require(StageForCore(37.0f) == HypothermiaStage::kNormal);
    Require(StageForCore(36.5f) == HypothermiaStage::kNormal);
    Require(StageForCore(36.49f) == HypothermiaStage::kChilled);
    Require(StageForCore(35.5f) == HypothermiaStage::kChilled);
    Require(StageForCore(35.49f) == HypothermiaStage::kMild);
    Require(StageForCore(34.49f) == HypothermiaStage::kModerate);
    Require(StageForCore(32.99f) == HypothermiaStage::kSevere);
    Require(StageForCore(29.99f) == HypothermiaStage::kCritical);

    ClimateState badState{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        HypothermiaStage::kCritical
    };
    const auto sanitized = SanitizeState(badState);
    Require(Near(sanitized.coreC, kNormalCoreC));
    Require(Near(sanitized.wetness01, 0.0f));
    Require(sanitized.stage == HypothermiaStage::kNormal);

    EnvironmentSample heavyRain{};
    heavyRain.ambientC = 5.0f;
    heavyRain.precipitation01 = 1.0f;
    const auto rainState = AdvanceClimate({}, heavyRain, {}, 120);
    Require(rainState.wetness01 > 0.30f);

    EnvironmentSample submerged{};
    submerged.submerged = true;
    ClimateState partlyWet{};
    partlyWet.wetness01 = 0.2f;
    const auto submergedState = Step(partlyWet, submerged, {}, 2.0f).state;
    Require(Near(submergedState.wetness01, 1.0f));

    EnvironmentSample indoors{};
    indoors.ambientC = 10.0f;
    indoors.indoors = true;
    ClimateState soaked{};
    soaked.wetness01 = 1.0f;
    const auto indoorDry = AdvanceClimate(soaked, indoors, {}, 120);

    EnvironmentSample heatedIndoor = indoors;
    heatedIndoor.heat01 = 1.0f;
    const auto heatedDry = AdvanceClimate(soaked, heatedIndoor, {}, 120);
    Require(heatedDry.wetness01 < indoorDry.wetness01);
    Require(Near(heatedDry.wetness01, 0.0f));

    EnvironmentSample dryCold{};
    dryCold.ambientC = -10.0f;
    const auto dryColdState = AdvanceClimate({}, dryCold, {}, 600);

    EnvironmentSample wetWindCold = dryCold;
    wetWindCold.windMps = 18.0f;
    wetWindCold.precipitation01 = 1.0f;
    const auto wetWindState = AdvanceClimate({}, wetWindCold, {}, 600);
    Require(wetWindState.coreC < dryColdState.coreC);

    ProtectionSample insulated{};
    insulated.insulation01 = 0.8f;
    const auto insulatedState = AdvanceClimate({}, dryCold, insulated, 600);
    Require(insulatedState.coreC > dryColdState.coreC);

    ProtectionSample powerArmor{};
    powerArmor.powerArmor = true;
    const auto powerArmorState = AdvanceClimate({}, dryCold, powerArmor, 600);
    Require(powerArmorState.coreC >= insulatedState.coreC);

    ClimateState chilled{};
    chilled.coreC = 35.0f;
    chilled.wetness01 = 0.5f;
    const auto unheatedState = AdvanceClimate(chilled, dryCold, insulated, 600);

    EnvironmentSample heated = dryCold;
    heated.shelter01 = 0.5f;
    heated.heat01 = 1.0f;
    const auto heatedState = AdvanceClimate(chilled, heated, insulated, 600);
    Require(heatedState.coreC > unheatedState.coreC);
    Require(Near(heatedState.coreC, kNormalCoreC));

    EnvironmentSample extremeCold{};
    extremeCold.ambientC = -20.0f;
    extremeCold.windMps = 20.0f;
    ClimateState saturated{};
    saturated.wetness01 = 1.0f;
    const auto extremeState = AdvanceClimate(saturated, extremeCold, {}, 1800);
    Require(extremeState.stage == HypothermiaStage::kCritical);

    const auto normalEffects = EffectsForStage(HypothermiaStage::kNormal);
    const auto moderateEffects = EffectsForStage(HypothermiaStage::kModerate);
    const auto criticalEffects = EffectsForStage(HypothermiaStage::kCritical);
    Require(Near(normalEffects.movementMultiplier, 1.0f));
    Require(moderateEffects.movementMultiplier < normalEffects.movementMultiplier);
    Require(criticalEffects.movementMultiplier < moderateEffects.movementMultiplier);
    Require(criticalEffects.healthDamagePerSecond > moderateEffects.healthDamagePerSecond);
    Require(EffectsForStage(HypothermiaStage::kNormal).strengthDelta == 0);

    EnvironmentSample comfortable{};
    comfortable.ambientC = 24.0f;
    comfortable.indoors = true;
    ClimateState recovering{};
    recovering.coreC = 35.0f;
    const auto recovered = AdvanceClimate(recovering, comfortable, {}, 1800);
    Require(recovered.coreC > recovering.coreC);
    Require(recovered.coreC <= kNormalCoreC);

    Require(kEmptyBottles.size() == 26);
    Require(kWaterSources.size() == 10);
    Require(kMaintainedVaultCells.size() == 2);
    Require(std::strcmp(kEmptyBottles[0].editorID, "NukaColaEmpty") == 0);
    Require(std::strcmp(kEmptyBottles.back().editorID, "DN050BabyBottle") == 0);
    Require(std::strcmp(kFilledWater.editorID, "WaterDirty") == 0);
    Require(kFilledWater.localID == 0x000366BF);
    Require(std::strcmp(kPurifiedWater.editorID, "WaterPurified") == 0);
    Require(kPurifiedWater.localID == 0x000366C0);
    Require(std::strcmp(kWaterPumpKeyword.editorID, "AnimFurnWaterPump") == 0);
    Require(kWaterPumpKeyword.localID == 0x000DCDD9);

    struct ExpectedCell
    {
        std::uint32_t localID;
        const char*   editorID;
    };
    const ExpectedCell expectedCells[]{
        { 0x0002A728, "Vault81" },
        { 0x000A9B62, "Vault81Entry" },
    };
    Require(sizeof(expectedCells) / sizeof(expectedCells[0]) == kMaintainedVaultCells.size());
    for (std::size_t i = 0; i < kMaintainedVaultCells.size(); ++i)
    {
        Require(kMaintainedVaultCells[i].localID == expectedCells[i].localID);
        Require(std::strcmp(kMaintainedVaultCells[i].editorID, expectedCells[i].editorID) == 0);
        Require(std::strcmp(kMaintainedVaultCells[i].plugin, "Fallout4.esm") == 0);
    }

    const std::uint32_t expectedBottles[]{
        0x0004835A, 0x000211DD, 0x000DF221, 0x000DF222, 0x000DF223, 0x000DF24D, 0x000DEDDF, 0x0014A138,
        0x000DEDE0, 0x000DEDE1, 0x000DEDED, 0x001BEAEE, 0x0003B55B, 0x0011EA9B, 0x0011EA9E, 0x0011EA9C,
        0x0011EA9D, 0x0011EAA0, 0x0011EA9F, 0x000DEDD9, 0x000DF031, 0x000DF032, 0x0017604F, 0x001A89A0,
        0x001A899B, 0x0009B4BA,
    };
    Require(sizeof(expectedBottles) / sizeof(expectedBottles[0]) == kEmptyBottles.size());
    for (std::size_t i = 0; i < kEmptyBottles.size(); ++i)
    {
        Require(kEmptyBottles[i].localID == expectedBottles[i]);
        Require(std::strcmp(kEmptyBottles[i].plugin, "Fallout4.esm") == 0);
    }

    struct ExpectedSource
    {
        const char*   plugin;
        std::uint32_t localID;
    };
    const ExpectedSource expectedSources[]{
        { "Fallout4.esm", 0x000B1CFE },
        { "Fallout4.esm", 0x0020DE62 },
        { "Fallout4.esm", 0x000C3676 },
        { "Fallout4.esm", 0x000F5D8D },
        { "Fallout4.esm", 0x001A4AA4 },
        { "Fallout4.esm", 0x000DB83B },
        { "DLCCoast.esm", 0x00039A92 },
        { "DLCworkshop03.esm", 0x0000251F },
        { "DLCworkshop03.esm", 0x00001D3D },
        { "DLCworkshop03.esm", 0x000049AD },
    };
    Require(sizeof(expectedSources) / sizeof(expectedSources[0]) == kWaterSources.size());
    for (std::size_t i = 0; i < kWaterSources.size(); ++i)
    {
        Require(kWaterSources[i].localID == expectedSources[i].localID);
        Require(std::strcmp(kWaterSources[i].plugin, expectedSources[i].plugin) == 0);
    }

    for (const auto& ref : kEmptyBottles)
        Require(ref.localID < 0x01000000);
    for (const auto& ref : kWaterSources)
        Require(ref.localID < 0x01000000);
    for (const auto& ref : kMaintainedVaultCells)
        Require(ref.localID < 0x01000000);

    for (std::size_t i = 0; i < kEmptyBottles.size(); ++i)
        for (std::size_t j = i + 1; j < kEmptyBottles.size(); ++j)
            Require(!(kEmptyBottles[i].localID == kEmptyBottles[j].localID &&
                      std::strcmp(kEmptyBottles[i].plugin, kEmptyBottles[j].plugin) == 0));
    for (std::size_t i = 0; i < kWaterSources.size(); ++i)
        for (std::size_t j = i + 1; j < kWaterSources.size(); ++j)
            Require(!(kWaterSources[i].localID == kWaterSources[j].localID &&
                      std::strcmp(kWaterSources[i].plugin, kWaterSources[j].plugin) == 0));
    for (std::size_t i = 0; i < kMaintainedVaultCells.size(); ++i)
        for (std::size_t j = i + 1; j < kMaintainedVaultCells.size(); ++j)
            Require(!(kMaintainedVaultCells[i].localID == kMaintainedVaultCells[j].localID &&
                      std::strcmp(kMaintainedVaultCells[i].plugin, kMaintainedVaultCells[j].plugin) == 0));

    const std::uint32_t resolvedSources[]{ 0x000B1CFE, 0x000DB83B };
    Require(IsWaterSource(0x000B1CFE, false, resolvedSources, 2));
    Require(!IsWaterSource(0x000F5D8D, false, resolvedSources, 2));
    Require(IsWaterSource(0x000F5D8D, true, resolvedSources, 2));
    Require(IsWaterSource(0x000F5D8D, true, nullptr, 0));
    Require(!IsWaterSource(0x000B1CFE, false, nullptr, 0));
    Require(!IsWaterSource(0, false, resolvedSources, 2));

    const std::uint32_t resolvedCells[]{ 0x0002A728, 0x000A9B62 };
    Require(IsMaintainedVaultCell(0x0002A728, resolvedCells, 2));
    Require(IsMaintainedVaultCell(0x000A9B62, resolvedCells, 2));
    Require(!IsMaintainedVaultCell(0x05226E, resolvedCells, 2));
    Require(!IsMaintainedVaultCell(0x1126B0, resolvedCells, 2));
    Require(!IsMaintainedVaultCell(0x00E4D0, resolvedCells, 2));
    Require(!IsMaintainedVaultCell(0, resolvedCells, 2));
    Require(!IsMaintainedVaultCell(0x0002A728, nullptr, 0));

    Require(FilledWaterFor(true) == FilledWater::kPurified);
    Require(FilledWaterFor(false) == FilledWater::kDirty);

    const std::uint32_t noBottles[]{ 0, 0, 0 };
    const std::uint32_t someBottles[]{ 0, 2, 1 };
    Require(FirstAvailableBottle(noBottles, 3) == kBottleNotFound);
    Require(FirstAvailableBottle(someBottles, 3) == 1);
    Require(FirstAvailableBottle(nullptr, 3) == kBottleNotFound);

    Require(Decide(true, true, false, true, 1) == Decision::kFill);
    Require(Decide(false, true, false, true, 1) == Decision::kDisabled);
    Require(Decide(false, false, true, false, 0) == Decision::kDisabled);
    Require(Decide(true, false, true, false, 0) == Decision::kNotWaterSource);
    Require(Decide(true, true, true, true, 1) == Decision::kVanillaSurvival);
    Require(Decide(true, true, true, false, 1) == Decision::kVanillaSurvival);
    Require(Decide(true, true, false, false, 1) == Decision::kNoModifier);
    Require(Decide(true, true, false, true, 0) == Decision::kNoEmptyBottle);

    using RadCorrectnessPolicy::GodModeClampedDrain;
    Require(Near(GodModeClampedDrain(50.0f, -10.0f, true, true), -10.0f));
    Require(Near(GodModeClampedDrain(5.0f, -10.0f, true, true), -4.0f));
    Require(Near(GodModeClampedDrain(5.0f, -4.0f, true, true), -4.0f));
    Require(Near(GodModeClampedDrain(1.0f, -3.0f, true, true), 0.0f));
    Require(Near(GodModeClampedDrain(0.5f, -3.0f, true, true), 0.0f));
    Require(Near(GodModeClampedDrain(50.0f, -10.0f, false, true), -10.0f));
    Require(Near(GodModeClampedDrain(5.0f, -10.0f, true, false), -10.0f));
    Require(Near(GodModeClampedDrain(5.0f, 10.0f, true, true), 10.0f));
    Require(Near(GodModeClampedDrain(1.0f, 10.0f, true, true), 10.0f));
    Require(Near(GodModeClampedDrain(5.0f, 0.0f, true, true), 0.0f));

    Require(SanitizeUiText("Raider Helm") == "Raider Helm");
    Require(SanitizeUiText("caf\xC3\xA9 25\xC2\xB0") == "caf\xC3\xA9 25\xC2\xB0");
    Require(SanitizeUiText("a\xF0\x9F\x98\x80" "b") == "a?b");
    Require(SanitizeUiText("x\xE4\xB8") == "x?");
    Require(SanitizeUiText("x\xC3") == "x?");
    Require(SanitizeUiText("a\tb\nc\x7F") == "a?b?c?");
    Require(SanitizeUiText("\x80z") == "?z");
    Require(SanitizeUiText("\xC0\xAF") == "??");
    Require(SanitizeUiText("\xE0\x80\xAF") == "???");
    Require(SanitizeUiText("\xED\xA0\x80") == "???");
    Require(SanitizeUiText("") == "");

    Require(!LoadSettleElapsed(0, 0, kLoadSettleMs));
    Require(!LoadSettleElapsed(2999, 0, kLoadSettleMs));
    Require(LoadSettleElapsed(3000, 0, kLoadSettleMs));
    Require(!LoadSettleElapsed(4999, 2000, kLoadSettleMs));
    Require(LoadSettleElapsed(5000, 2000, kLoadSettleMs));
    Require(!LoadSettleElapsed(1000, 2000, kLoadSettleMs));
    Require(LoadSettleElapsed(0, 0, 0));
    Require(!LoadSettleElapsed(11 * kEngineTickMs, 0, kLoadSettleMs));
    Require(LoadSettleElapsed(12 * kEngineTickMs, 0, kLoadSettleMs));
    Require(kLoadSettleMs / kEngineTickMs == 12);

    Require(CompanionBridgeReady(false, false, 0));
    Require(!CompanionBridgeReady(true, false, 0));
    Require(!CompanionBridgeReady(true, false, kCompanionBridgeMaxTicks - 1));
    Require(CompanionBridgeReady(true, false, kCompanionBridgeMaxTicks));
    Require(CompanionBridgeReady(true, true, 0));
    Require(kCompanionBridgeMaxTicks * kEngineTickMs == 10000);

    Require(!IsLiveDomReady(0, 0));
    Require(!IsLiveDomReady(0, 5));
    Require(!IsLiveDomReady(5, 0));
    Require(IsLiveDomReady(5, 5));
    Require(!IsLiveDomReady(5, 6));

    for (int type : { -1, 0, 1, 2, 3, 4, 5 })
        for (int phase : { kPhaseIdle, kPhaseAftermath, kPhaseApproach })
            Require(StormSkyChoice(type, phase) == StormSkyKind::None);
    for (int type : { -1, 5 })
        for (int phase : { kPhaseWarning, kPhaseActive })
            Require(StormSkyChoice(type, phase) == StormSkyKind::None);
    Require(StormSkyChoice(0, kPhaseWarning) == StormSkyKind::None);
    Require(StormSkyChoice(1, kPhaseWarning) == StormSkyKind::None);
    Require(StormSkyChoice(2, kPhaseWarning) == StormSkyKind::None);
    Require(StormSkyChoice(3, kPhaseWarning) == StormSkyKind::None);
    Require(StormSkyChoice(4, kPhaseWarning) == StormSkyKind::CustomWeather);
    Require(StormSkyChoice(0, kPhaseActive) == StormSkyKind::VanillaRadstorm);
    Require(StormSkyChoice(1, kPhaseActive) == StormSkyKind::CustomWeather);
    Require(StormSkyChoice(2, kPhaseActive) == StormSkyKind::VanillaRadstorm);
    Require(StormSkyChoice(3, kPhaseActive) == StormSkyKind::CustomWeather);
    Require(StormSkyChoice(4, kPhaseActive) == StormSkyKind::CustomWeather);

    Require(VanillaSkyStormBedAllowed(0, kPhaseActive));
    Require(VanillaSkyStormBedAllowed(2, kPhaseActive));
    Require(!VanillaSkyStormBedAllowed(0, kPhaseWarning));
    Require(!VanillaSkyStormBedAllowed(2, kPhaseApproach));
    Require(!VanillaSkyStormBedAllowed(1, kPhaseActive));
    Require(!VanillaSkyStormBedAllowed(-1, kPhaseActive));

    Require(!SpawnGhoulsOnRadWeatherEntry(true, true, false, kPhaseIdle));
    Require(!SpawnGhoulsOnRadWeatherEntry(true, true, false, kPhaseWarning));
    Require(SpawnGhoulsOnRadWeatherEntry(true, true, false, kPhaseActive));
    Require(SpawnGhoulsOnRadWeatherEntry(true, true, true, kPhaseIdle));
    Require(SpawnGhoulsOnRadWeatherEntry(true, false, false, kPhaseWarning));
    Require(SpawnGhoulsOnRadWeatherEntry(true, false, false, kPhaseIdle));
    Require(!SpawnGhoulsOnRadWeatherEntry(false, false, false, kPhaseActive));
    Require(!SpawnGhoulsOnRadWeatherEntry(false, true, true, kPhaseActive));

    Require(!InVanillaRadstormCooldown(1000.0, -1.0e30, 3600.0));
    Require(InVanillaRadstormCooldown(3599.0, 0.0, 3600.0));
    Require(!InVanillaRadstormCooldown(3600.0, 0.0, 3600.0));
    Require(InVanillaRadstormCooldown(10.0, 5.0, 60.0));
    Require(!InVanillaRadstormCooldown(0.0, 0.0, 0.0));
    Require(!InVanillaRadstormCooldown(5.0, 0.0, 0.0));

    Require(ShouldStampRadstormCooldown(0, kPhaseActive));
    Require(ShouldStampRadstormCooldown(2, kPhaseActive));
    Require(!ShouldStampRadstormCooldown(0, kPhaseWarning));
    Require(!ShouldStampRadstormCooldown(2, kPhaseWarning));
    Require(!ShouldStampRadstormCooldown(0, kPhaseAftermath));
    Require(!ShouldStampRadstormCooldown(1, kPhaseActive));
    Require(!ShouldStampRadstormCooldown(3, kPhaseActive));
    Require(!ShouldStampRadstormCooldown(4, kPhaseActive));
    Require(!ShouldStampRadstormCooldown(-1, kPhaseActive));

    Require(Near(ResolveStormLeadMinutes("", ""), 3.0f));
    Require(Near(ResolveStormLeadMinutes("", "5"), 5.0f));
    Require(Near(ResolveStormLeadMinutes("2", "5"), 2.0f));
    Require(Near(ResolveStormLeadMinutes("", "-1"), 3.0f));
    Require(Near(ResolveStormLeadMinutes("abc", ""), 3.0f));
    Require(Near(ResolveStormLeadMinutes("0", "5"), 0.0f));
    Require(Near(ResolveStormLeadMinutes("", "inf"), 3.0f));
    Require(Near(ResolveStormLeadMinutes("2.5 ; lead in minutes", ""), 2.5f));
    Require(Near(ResolveStormLeadMinutes("2.5abc", ""), 3.0f));

    return 0;
}
