// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "ModUtils.h"
#include "minhook.h"

#include <delayimp.h>
#pragma comment (lib, "delayimp")
#pragma comment (lib, "../bin/MinHook.x64.lib")
//#pragma comment (lib, "../bin/MinHook.x64.dll")

//std::string GetCWD()
//{
//    constexpr SIZE_T len = 1000;
//    char buf[len];
//    if (GetCurrentDirectoryA(len, buf))
//    {
//        return std::string(buf);
//    }
//    return std::string();
//}
//
//std::string SetEnv()
//{
//    constexpr SIZE_T len = 10000;
//    char buf[len];
//    std::string pathstr;
//    if (GetEnvironmentVariableA("PATH", buf, len) < len)
//    {
//        pathstr = std::string(buf);
//        pathstr += ";" + GetCWD() + "\\mods\\bin\\";
//        SetEnvironmentVariableA("PATH", pathstr.c_str());
//    }
//    return pathstr;
//}

constexpr ULONG_PTR OFFSET_FN_APPLYWOBBLE = 0x3B5340;

void (*tram_CamPos)(LPVOID);
void hk_CamPos(LPVOID p)
{
    float* pWobbleAlpha = reinterpret_cast<float*>(ULONG_PTR(p) + 0x140);
    *pWobbleAlpha = 1.f;
    tram_CamPos(p);
}
DWORD WINAPI Main(LPVOID lpParams)
{
    //SetEnv();
    LoadLibraryA("mods\\bin\\MinHook.x64.dll");

    MH_Initialize();
    ULONG_PTR baseAddr = ModUtils::GetProcessBaseAddress(GetCurrentProcessId());

    LPVOID pTarget = reinterpret_cast<LPVOID>(baseAddr + OFFSET_FN_APPLYWOBBLE);
    ModUtils::Log("target = %p", pTarget);
    MH_STATUS s = MH_CreateHook(pTarget, &hk_CamPos, reinterpret_cast<LPVOID*>(&tram_CamPos));
    ModUtils::Log("%s", MH_StatusToString(s));
    s = MH_EnableHook(pTarget);
    ModUtils::Log("%s", MH_StatusToString(s));

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
        CreateThread(0, 0, &Main, 0, 0, 0);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

