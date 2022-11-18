// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"

#include "../include/ModUtils.h"
using ModUtils::MASK;

#include "../include/HookUtils.h"

#include <vector>

extern "C" 
{
    void OverrideRestrictedArea();
    uintptr_t ReturnAddress;

    void GetStructAddress();
}

std::vector<std::shared_ptr<UHookRelativeIntermediate>> Hooks;

DWORD WINAPI MainThread(LPVOID lpParam)
{
    {
        std::vector<uint16_t> pattern({ 0x0F, 0xB6, MASK, MASK, 0xD3, MASK, 0x49, 0x85, 0x53, 0x28, 0x74, MASK, MASK, 0x8B, MASK, MASK, MASK, 0x85 });
        Hooks.push_back(std::make_shared<UHookRelativeIntermediate>(pattern, 6, &OverrideRestrictedArea, 0, &ReturnAddress, "HookDisableRestrictions"));
    }

    for (std::shared_ptr<UHookRelativeIntermediate> pHook : Hooks)
    {
        pHook->Enable();
    }

    {
        std::vector<uint16_t> pattern({ 0x75, MASK, 0x48, 0xB8, MASK, MASK, MASK, MASK, MASK, MASK, MASK, MASK, 0x48, 0x85, MASK, MASK, 0x74, 0x33, 0x48, 0x8B, MASK, MASK, 0x48, 0x85, MASK, 0x74, MASK, 0xBA, MASK, MASK, MASK, MASK, 0x66, 0x90 });
        uintptr_t address = ModUtils::SigScan(pattern);
        if (address)
        {
            address += 16;
            ModUtils::MemSet(address, 0xEB, 1);
        }
    }

    ModUtils::CloseLog();
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &MainThread, nullptr, 0, nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

