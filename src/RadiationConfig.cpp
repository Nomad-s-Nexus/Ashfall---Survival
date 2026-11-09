#include "pch.h"
#include "RadiationConfig.h"

bool RadiationConfig::LoadFromFile(const std::string& basePath)
{
    auto logger = spdlog::get("FalloutAnomaly");

    try
    {
        std::ifstream thresholdsFile(basePath + "/thresholds.json");
        if (!thresholdsFile.is_open())
        {
            logger->error("thresholds.json missing/unreadable at '{}/thresholds.json' — radiation "
                          "stage debuffs and Pip-Boy stage perks are DISABLED this session.", basePath);
            return true;
        }

        json thresholdsJson;
        thresholdsFile >> thresholdsJson;
        if (thresholdsJson.contains("stages"))
        for (const auto& [level, data] : thresholdsJson["stages"].items())
        {
            uint32_t radLevel = std::stoul(level);
            StageConfig stage;
            stage.name = data.value("name", std::string{});
            _stages[radLevel] = stage;
        }
        if (_stages.empty())
            logger->error("thresholds.json parsed but contains no 'stages' — stage debuffs DISABLED.");
        else
            logger->info("Loaded {} radiation stages from config.", _stages.size());

        return true;
    }
    catch (const std::exception& e)
    {
        logger->error("Failed to load config: {}", e.what());
        return false;
    }
}

const StageConfig* RadiationConfig::GetStageConfig(uint32_t radLevel) const
{
    if (_stages.empty())
    {
        return nullptr;
    }

    auto it = _stages.upper_bound(radLevel);
    if (it == _stages.begin())
    {
        return nullptr;
    }
    --it;
    return &it->second;
}
