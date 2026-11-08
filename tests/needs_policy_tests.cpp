#include "NeedsPolicy.h"

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
    using namespace RadNeedsPolicy;

    Require(Near(Clamp01(0.25f), 0.25f));
    Require(Near(Clamp01(-1.0f), 0.0f));
    Require(Near(Clamp01(2.0f), 1.0f));
    Require(Near(Clamp01(std::numeric_limits<float>::quiet_NaN()), 0.0f));
    Require(Near(Clamp01(std::numeric_limits<float>::infinity()), 0.0f));

    NeedsState state;
    state.hunger01 = 1.4f;
    state.thirst01 = -0.2f;
    state.sleepDebt01 = 0.5f;
    const auto safe = Sanitize(state);
    Require(Near(safe.hunger01, 1.0f));
    Require(Near(safe.thirst01, 0.0f));
    Require(Near(safe.sleepDebt01, 0.5f));

    NeedsSnapshot snapshot;
    snapshot.pools = state;
    snapshot.risk01 = 1.8f;
    snapshot.diseasePacked = 22;
    auto clamped = Sanitize(snapshot);
    Require(Near(clamped.risk01, 1.0f));
    Require(Near(clamped.pools.hunger01, 1.0f));
    Require(clamped.diseasePacked == 22);

    snapshot.diseasePacked = 99;
    Require(Sanitize(snapshot).diseasePacked == kDiseaseNone);
    snapshot.diseasePacked = -4;
    Require(Sanitize(snapshot).diseasePacked == kDiseaseNone);
    snapshot.diseasePacked = kDiseasePackedMax;
    Require(Sanitize(snapshot).diseasePacked == kDiseasePackedMax);
    snapshot.diseasePacked = kDiseaseNone;
    Require(Sanitize(snapshot).diseasePacked == kDiseaseNone);

    NeedsSnapshot decoded = DecodeDisease(NeedsSnapshot{}, 10.0f);
    Require(decoded.diseasePacked == 10 && Near(decoded.diseaseOnset01, 0.0f));
    decoded = DecodeDisease(NeedsSnapshot{}, 10.0f + 0.45f * 0.6f);
    Require(decoded.diseasePacked == 10 && Near(Sanitize(decoded).diseaseOnset01, 0.6f));
    decoded = DecodeDisease(NeedsSnapshot{}, 20.45f);
    Require(decoded.diseasePacked == 20 && Near(Sanitize(decoded).diseaseOnset01, 1.0f));
    decoded = DecodeDisease(NeedsSnapshot{}, 12.0f);
    Require(decoded.diseasePacked == 12 && Near(Sanitize(decoded).diseaseOnset01, 0.0f));
    decoded.diseasePacked = 11;
    decoded.diseaseOnset01 = 0.7f;
    Require(Near(Sanitize(decoded).diseaseOnset01, 0.0f));
    Require(DecodeDisease(NeedsSnapshot{}, std::numeric_limits<float>::quiet_NaN()).diseasePacked == 0);

    return 0;
}
