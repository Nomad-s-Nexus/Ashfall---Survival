#pragma once

class OutbreakRuntime
{
public:
    static OutbreakRuntime& GetSingleton()
    {
        static OutbreakRuntime instance;
        return instance;
    }

    void Update();
    void Reset();

private:
    OutbreakRuntime() = default;
    void Resolve();

    void* _faction{ nullptr };
    void* _companion{ nullptr };
    void* _scanner{ nullptr };
    void* _vanillaScanner{ nullptr };
};
