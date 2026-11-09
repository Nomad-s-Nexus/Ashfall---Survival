#pragma once

#include <functional>

namespace Hooks
{
    bool InstallMainThreadDispatcher();

    bool InstallUpdateHook();
    bool InstallInputHandler();
    void SetStormIMADFadeActive(bool active) noexcept;
    void QueueStormVisuals();

    bool QueueMainThreadTask(std::function<void()> task);

    void ResetQueuedTaskFlags() noexcept;

    void Shutdown() noexcept;

    bool IsGameMainThread();
}
