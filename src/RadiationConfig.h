#pragma once

struct StageConfig
{
    std::string name;
};

class RadiationConfig
{
public:
    static RadiationConfig& GetSingleton()
    {
        static RadiationConfig instance;
        return instance;
    }

    bool LoadFromFile(const std::string& basePath);

    const StageConfig* GetStageConfig(uint32_t radLevel) const;

private:
    RadiationConfig() = default;
    ~RadiationConfig() = default;

    std::map<uint32_t, StageConfig> _stages;
};
