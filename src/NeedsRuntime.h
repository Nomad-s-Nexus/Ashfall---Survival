#pragma once

#include "NeedsPolicy.h"

#include <mutex>

class NeedsRuntime
{
public:
    static NeedsRuntime& GetSingleton()
    {
        static NeedsRuntime instance;
        return instance;
    }

    void Update();
    void Reset();

private:
    NeedsRuntime() = default;
    void ResolveGlobals();
    void UpdateSickCough(const RadNeedsPolicy::NeedsSnapshot& snapshot);

    mutable std::mutex _mutex;
    void* _hunger{ nullptr };
    void* _thirst{ nullptr };
    void* _sleep{ nullptr };
    void* _risk{ nullptr };
    void* _disease{ nullptr };
    int _coughTick{ 0 };
};
