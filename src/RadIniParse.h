#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace RadIniParse
{
    inline bool ParseNumber(std::string_view a_text, std::uint32_t a_max, std::uint32_t& a_out)
    {
        auto text = a_text;
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            text.remove_prefix(1);
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
            text.remove_suffix(1);
        if (text.empty() || text.front() == '+')
            return false;

        const bool hexPrefix = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
        if (!hexPrefix && text.size() > 1 && text[0] == '0')
            return false;

        const std::string copy(text);
        char*             end = nullptr;
        const long        value = std::strtol(copy.c_str(), &end, 0);
        if (!end || end == copy.c_str() || *end != '\0' || value < 0)
            return false;
        if (static_cast<unsigned long>(value) > a_max)
            return false;
        a_out = static_cast<std::uint32_t>(value);
        return true;
    }
}
