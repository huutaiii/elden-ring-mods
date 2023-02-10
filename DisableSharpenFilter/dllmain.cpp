// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include "../include/ModUtils.h"

#include <vector>

constexpr char16_t SHADER_NAME[] = u"SharpenFilter.ppo";

std::vector<uint8_t> UTF16ToAOB(std::u16string s16)
{
    std::vector<uint8_t> out;
    for (char16_t c : s16)
    {
        out.push_back(c & 0xff);
        out.push_back(c >> 8);
    }
    return out;
}

DWORD WINAPI MainThread(LPVOID params)
{
    ModUtils::settings.bLogProcess = true;

    std::vector<uint8_t> pattern = UTF16ToAOB(std::u16string(SHADER_NAME));
    uintptr_t address = ModUtils::SigScan(std::vector<uint16_t>(pattern.begin(), pattern.end()), std::string(pattern.begin(), pattern.end()));
    if (address)
    {
        ModUtils::MemSet(address, 0, pattern.size());
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

