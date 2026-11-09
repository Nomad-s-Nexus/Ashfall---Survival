#pragma once

namespace RE
{
    class TESForm;

    namespace BSScript
    {
        class IVirtualMachine;
    }
}

namespace RadReliefConfig
{
    void InstallPapyrusBridge();

    void ActivateOnGameThread();

    int ReliefFlags(const RE::TESForm* a_form);
}
