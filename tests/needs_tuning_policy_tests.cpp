#include "RadNeedsTuningPolicy.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace
{
    void Require(bool condition)
    {
        if (!condition)
            std::abort();
    }

    bool Near(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }
}

int main()
{
    using namespace RadNeedsTuningPolicy;

    Require(FindKey("RateMultiplier") == 0);
    Require(FindKey("HourlyHunger") == 1);
    Require(FindKey("DrinkScale") == static_cast<int>(kDrinkScale));
    Require(FindKey("SleepRecovery") == static_cast<int>(kSleepRecovery));
    Require(FindKey("RiskContaminated") == 15);
    Require(FindKey("OutbreakEnabled") == 16 && FindKey("OutbreakEnabled") == static_cast<int>(kOutbreakEnabled));
    Require(FindKey("OutbreakSpread") == 17 && FindKey("OutbreakDeaths") == 18);
    Require(FindKey("OutbreakAmbient") == 19 && FindKey("OutbreakAmbient") == static_cast<int>(kPapyrusKeys) - 1);
    Require(kPapyrusKeys == 20);
    Require(FindKey("FilterEnabled") == static_cast<int>(kPapyrusKeys));
    Require(FindKey("AutoEquipMask") == static_cast<int>(kAutoEquipMask));
    Require(FindKey("AutoEquipInCombat") == static_cast<int>(kAutoEquipInCombat));
    Require(FindKey("AlwaysShowNeeds") == static_cast<int>(kAlwaysShowNeeds));
    Require(FindKey("WaterRequireSneak") == static_cast<int>(kWaterRequireSneak));
    Require(FindKey("WaterRequireSneak") == static_cast<int>(kHypothermiaEffects) - 1);
    Require(FindKey("HypothermiaEffects") == static_cast<int>(kHypothermiaEffects));
    Require(FindKey("HypothermiaDamage") == static_cast<int>(kHypothermiaDamage));
    Require(FindKey("DiseaseCauseScale") == static_cast<int>(kDiseaseCauseScale));
    Require(FindKey("DiseaseCauseScale") == static_cast<int>(kDiseaseLethal) - 1);
    Require(FindKey("DiseaseLethal") == static_cast<int>(kDiseaseLethal) && kSpecs[kDiseaseLethal].toggle);
    Require(FindKey("DiseaseLethalHours") == static_cast<int>(kLimbInjuriesEnabled) - 1);
    Require(Near(kSpecs[kDiseaseLethalHours].reference, 24.0f) && !kSpecs[kDiseaseLethalHours].toggle);
    Require(kSpecs[kHypothermiaEffects].toggle);
    Require(FindKey("LimbInjuriesEnabled") == static_cast<int>(kLimbInjuriesEnabled));
    Require(FindKey("LimbInjuryScale") == static_cast<int>(kLimbInjuryScale));
    Require(FindKey("LimbHealRate") == static_cast<int>(kLimbHealRate));
    Require(FindKey("LimbBleedRate") == static_cast<int>(kLimbBleedRate));
    Require(FindKey("LimbCrippleFractures") == static_cast<int>(kLimbCrippleFractures));
    Require(FindKey("LimbCrippleFractures") == static_cast<int>(kCount) - 1);
    Require(kCount == kKeyCount);
    Require(FindKey("hourlyhunger") == -1);
    Require(FindKey("") == -1);

    float out = 0.0f;
    Require(Accept(1, 0.03f, out) && Near(out, 0.03f));
    Require(Accept(1, 9.0f, out) && Near(out, 0.5f));
    Require(Accept(kDiseasesEnabled, 0.7f, out) && Near(out, 1.0f));
    Require(Accept(kDiseasesEnabled, 0.2f, out) && Near(out, 0.0f));
    Require(Accept(kWetnessDryRate, 0.0f, out) && Near(out, 0.1f));
    Require(Accept(kOutbreakEnabled, 0.7f, out) && Near(out, 1.0f));
    Require(Accept(kOutbreakDeaths, 0.2f, out) && Near(out, 0.0f));
    Require(Accept(kOutbreakSpread, 9.0f, out) && Near(out, 5.0f));
    Require(Accept(kOutbreakAmbient, 1.0f, out) && Near(out, 0.05f));
    Require(Near(kSpecs[kOutbreakAmbient].reference, 0.002f) && Near(kSpecs[kOutbreakSpread].reference, 1.0f));
    Require(Accept(kStormMaxMinutes, 9999.0f, out) && Near(out, 480.0f));
    Require(Accept(1, -1.0f, out) && Near(out, 0.0f));
    Require(!Accept(1, std::numeric_limits<float>::quiet_NaN(), out));
    Require(!Accept(1, std::numeric_limits<float>::infinity(), out));
    Require(!Accept(kCount, 0.1f, out));
    Require(Accept(kHypothermiaDamage, 3.5f, out) && Near(out, 3.0f));
    Require(Accept(kDiseaseCauseScale, -1.0f, out) && Near(out, 0.0f));
    Require(Accept(kDiseaseLethal, 0.2f, out) && Near(out, 0.0f));
    Require(Accept(kDiseaseLethalHours, 0.0f, out) && Near(out, 1.0f));
    Require(Accept(kDiseaseLethalHours, 999.0f, out) && Near(out, 240.0f));

    Require(ParseValue("0.0300", out) && Near(out, 0.03f));
    Require(ParseValue("  1 ", out) && Near(out, 1.0f));
    Require(ParseValue("-0.5", out) && Near(out, -0.5f));
    Require(!ParseValue("", out));
    Require(!ParseValue("   ", out));
    Require(!ParseValue("0.1abc", out));
    Require(!ParseValue("nan", out));
    Require(!ParseValue("inf", out));
    Require(!ParseValue("1e3", out));
    Require(!ParseValue("0x10", out));
    Require(!ParseValue("1.2.3", out));

    for (const auto& spec : kSpecs)
    {
        Require(spec.min <= spec.reference && spec.reference <= spec.max);
        Require(!spec.toggle || (spec.min == 0.0f && spec.max == 1.0f));
        Require(FindKey(spec.key) >= 0 && kSpecs[static_cast<std::size_t>(FindKey(spec.key))].key == spec.key);
    }

    return 0;
}
