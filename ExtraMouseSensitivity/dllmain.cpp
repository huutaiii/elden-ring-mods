// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include "ModUtils.h"
#include "minhook.h"
#include "INIReader.h"
#pragma comment (lib,"../bin/MinHook.x64.lib")

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

// offsets for 1.09 only
constexpr ULONG_PTR OFFSET_FN_READ_H = 0xDEB730;
constexpr ULONG_PTR OFFSET_FN_READ_V = 0xDEB7A0;

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

    ULONG_PTR baseAddr = ModUtils::GetProcessBaseAddress(GetCurrentProcessId());

    LPVOID pTargetH = reinterpret_cast<LPVOID>(baseAddr + OFFSET_FN_READ_H);
    LPVOID pTargetV = reinterpret_cast<LPVOID>(baseAddr + OFFSET_FN_READ_V);
    MH_CreateHook(pTargetH, &hk_GetMouseDeltaH, reinterpret_cast<LPVOID*>(&tram_GetMouseDeltaH));
    MH_CreateHook(pTargetV, &hk_GetMouseDeltaV, reinterpret_cast<LPVOID*>(&tram_GetMouseDeltaV));
    MH_QueueEnableHook(pTargetH);
    MH_QueueEnableHook(pTargetV);
    MH_ApplyQueued();

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

