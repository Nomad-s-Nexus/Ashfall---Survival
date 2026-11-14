#pragma once

#include "RadNeedsTuningPolicy.h"

#include <string>
#include <string_view>

namespace RadNeedsTuning
{
    void Install();

    bool Handle(std::string_view a_command);

    std::string Json();

    void SetLive(RadNeedsTuningPolicy::Key a_key, float a_value);

    float Value(RadNeedsTuningPolicy::Key a_key, float a_fallback);
    inline bool On(RadNeedsTuningPolicy::Key a_key, bool a_fallback)
    {
        return Value(a_key, a_fallback ? 1.0f : 0.0f) >= 0.5f;
    }
}
