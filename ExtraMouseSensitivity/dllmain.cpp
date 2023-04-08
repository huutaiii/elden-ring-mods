// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include "ModUtils.h"
#include "minhook.h"
#include "INIReader.h"
#pragma comment (lib,"../bin/MinHook.x64.lib")

using ModUtils::MASK;

struct FConfig
{
    float SpeedMulH = 1.f;
    float SpeedMulV = 1.f;

    void ReadINI()
    {
        INIReader ini(ModUtils::GetModuleFolderPath() + "\\config.ini");
        if (ini.ParseError() != 0)
        {
            ModUtils::RaiseError("Cannot read config file!");
            return;
        }
        SpeedMulH = ini.GetReal("mouse-sensitivity", "multiplier-horizontal", SpeedMulH);
        SpeedMulV = ini.GetReal("mouse-sensitivity", "multiplier-vertical", SpeedMulV);
    }
} Config;

float (*tram_GetMouseDeltaH)(LPVOID);
float hk_GetMouseDeltaH(LPVOID p)
{
    return tram_GetMouseDeltaH(p) * Config.SpeedMulH;
}

float (*tram_GetMouseDeltaV)(LPVOID);
float hk_GetMouseDeltaV(LPVOID p)
{
    return tram_GetMouseDeltaV(p) * Config.SpeedMulV;
}

// 48 8D 4D ?? E8 ???????? 0F28 F0 48 8D 4D ?? E8 ???????? F3 0F11 45 07 F3 0F11 75 0B
std::vector<uint16_t> PATTERN_MOUSE_INPUT_CALLER = { 0x48, 0x8D, 0x4D, MASK, 0xE8, MASK, MASK, MASK, MASK, 0x0F, 0x28, 0xF0, 0x48, 0x8D, 0x4D, MASK, 0xE8, MASK, MASK, MASK, MASK, 0xF3, 0x0F, 0x11, 0x45, 0x07, 0xF3, 0x0F, 0x11, 0x75, 0x0B };

DWORD WINAPI MainThread(LPVOID lpParams)
{
    HMODULE mh_module = LoadLibraryA("mods\\bin\\MinHook.x64.dll");
    if (!mh_module)
    {
        // it seems the game crashes before this can show up.
        ModUtils::RaiseError("Cannot load MinHook.x64");
    }
    MH_Initialize();

    Config.ReadINI();
    ModUtils::Log("Using SpeedMulH = %f", Config.SpeedMulH);
    ModUtils::Log("Using SpeedMulV = %f", Config.SpeedMulV);

    ULONG_PTR baseAddr = ModUtils::GetProcessBaseAddress(GetCurrentProcessId());

    LPVOID pTargetH = nullptr;
    LPVOID pTargetV = nullptr;

    UINT_PTR pScan = ModUtils::SigScan(PATTERN_MOUSE_INPUT_CALLER);
    if (pScan)
    {
        static constexpr SIZE_T CALL_SIZE = 5;
        {
            UINT_PTR CallH = pScan + 4;
            UINT_PTR RelAddrH = *reinterpret_cast<UINT32*>(CallH + 1);
            pTargetH = reinterpret_cast<LPVOID>(CallH + 5 + RelAddrH);

            ModUtils::Log("Mouse input H module offset = %p", reinterpret_cast<UINT_PTR>(pTargetH) - baseAddr);
        }

        {
            UINT_PTR CallV = pScan + 16;
            UINT_PTR RelAddrV = *reinterpret_cast<UINT32*>(CallV + 1);
            pTargetV = reinterpret_cast<LPVOID>(CallV + 5 + RelAddrV);

            ModUtils::Log("Mouse input V module offset = %p", reinterpret_cast<UINT_PTR>(pTargetV) - baseAddr);
        }
    }

    if (pTargetH && pTargetV)
    {
        MH_CreateHook(pTargetH, &hk_GetMouseDeltaH, reinterpret_cast<LPVOID*>(&tram_GetMouseDeltaH));
        MH_CreateHook(pTargetV, &hk_GetMouseDeltaV, reinterpret_cast<LPVOID*>(&tram_GetMouseDeltaV));
        MH_QueueEnableHook(pTargetH);
        MH_QueueEnableHook(pTargetV);
        MH_ApplyQueued();
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
        CreateThread(0, 0, &MainThread, 0, 0, 0);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

