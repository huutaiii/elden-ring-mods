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
        std::vector<uint16_t> pattern({ 0xE8, MASK, MASK, MASK, MASK, 0x84, 0xC0, 0x74, MASK, 0x44, 0x0F, 0xB6, MASK, 0x33, MASK, 0x48, 0x8B, MASK, 0xE8, MASK, MASK, MASK, MASK, 0x44, 0x0F, 0xB6 });
        uintptr_t address = ModUtils::SigScan(pattern);
        if (address)
        {
            address += 7;
            ModUtils::MemSet(address, 0xEB, 1);
        }
    }

    for (std::shared_ptr<UHookRelativeIntermediate> pHook : Hooks)
    {
        pHook->Enable();
    }

    // unblock spell casting
    {
        std::vector<uint16_t> pattern({ 0x75, MASK, 0x48, 0xB8, MASK, MASK, MASK, MASK, MASK, MASK, MASK, MASK, 0x48, 0x85, MASK, MASK, 0x74, 0x33, 0x48, 0x8B, MASK, MASK, 0x48, 0x85, MASK, 0x74, MASK, 0xBA, MASK, MASK, MASK, MASK, 0x66, 0x90 });
        uintptr_t address = ModUtils::SigScan(pattern);
        if (address)
        {
            address += 16;
            ModUtils::MemSet(address, 0xEB, 1);
        }
    }

    // fix the hud
    {
        std::vector<uint16_t> pattern = { 0xE8, MASK, MASK, MASK, MASK, 0x88, 0x44, MASK, MASK, 0x4D, 0x8D, MASK, MASK, MASK, MASK, MASK, 0x4D, 0x85, MASK, 0x0F, 0x84, MASK, MASK, MASK, MASK, 0x49, 0x8B, MASK, 0xE8 };
        uintptr_t address = ModUtils::SigScan(pattern, false, "", true);
        if (address)
        {
            ModUtils::MemSet(address, 0x90, 5);
            const char* xor_al_al = "\x30\xC0";
            ModUtils::MemCopy(address, reinterpret_cast<uintptr_t>(xor_al_al), 2);
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

