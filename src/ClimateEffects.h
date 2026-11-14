#pragma once

#include "ClimatePolicy.h"

namespace ClimateEffects
{
    void InstallPapyrusBridge();

    void OnTick(RadClimatePolicy::HypothermiaStage stage, bool liveTick);

    [[nodiscard]] float WarmthFloor01(float gameDays) noexcept;

    void Resend() noexcept;
    void Reset() noexcept;
}
