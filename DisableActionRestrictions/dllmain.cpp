// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"

#include "../include/ModUtils.h"
using ModUtils::MASK;

#include "../include/HookUtils.h"

#include <vector>

extern "C" void OverrideRestrictedArea();

DWORD WINAPI MainThread(LPVOID lpParam)
{
    std::vector<uint16_t> pattern({ 0x44, 0x0F, 0xB7, MASK, 0x48, 0xB8, MASK, MASK, MASK, MASK, MASK, MASK, MASK, MASK, 0x66, 0x45, 0x85, MASK, MASK, 0x8B, MASK, MASK, 0x8B, MASK });
    uintptr_t address = ModUtils::SigScan(pattern);
    if (address)
    {
        UHookAbsoluteNoCopy(reinterpret_cast<void*>(address), &OverrideRestrictedArea).Enable();
    }

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

