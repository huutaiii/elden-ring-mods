// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include "MinHook.h"
#pragma comment(lib,"../bin/MinHook.x64.lib")

#include <glm/gtc/type_ptr.hpp>

using ModUtils::MASK;

struct FConfig
{
    bool bAlwaysTurn = false;
    bool bUseLookAt = false;
} Config;

/*
eldenring.exe+3AFDD8 - 83 F8 07              - cmp eax,07
eldenring.exe+3AFDDB - 0F87 92040000         - ja eldenring.exe+3B0273
eldenring.exe+3AFDE1 - 41 8B 8C 85 CC023B00  - mov ecx,[r13+rax*4+003B02CC]
eldenring.exe+3AFDE9 - 49 03 CD              - add rcx,r13
eldenring.exe+3AFDEC - FF E1                 - jmp rcx
eldenring.exe+3AFDEE - 48 8B 4F 60           - mov rcx,[rdi+60]
eldenring.exe+3AFDF2 - 4D 8B CE              - mov r9,r14
eldenring.exe+3AFDF5 - 4C 8B C6              - mov r8,rsi
eldenring.exe+3AFDF8 - 41 0F28 C9            - movaps xmm1,xmm9
eldenring.exe+3AFDFC - E8 DF410000           - call eldenring.exe+3B3FE0

83 F8 07 0F87 ??????00 41 8B 8C 85 ???????? 49 03 CD FF E1 48 8B 4F ?? 4D 8B CE 4C 8B C6 41 0F28 C9 E8 ??????00
*/
std::vector<UINT16> PATTERN_CAMERA_TICK = { 0x83, 0xF8, 0x07, 0x0F, 0x87, MASK, MASK, MASK, 0x00, 0x41, 0x8B, 0x8C, 0x85, MASK, MASK, MASK, MASK, 0x49, 0x03, 0xCD, 0xFF, 0xE1, 0x48, 0x8B, 0x4F, MASK, 0x4D, 0x8B, 0xCE, 0x4C, 0x8B, 0xC6, 0x41, 0x0F, 0x28, 0xC9, 0xE8, MASK, MASK, MASK, 0x00 };

/*
eldenring.exe+708AD8 - E8 A3C0CEFF           - call eldenring.exe+3F4B80
eldenring.exe+708ADD - 80 BE 8E290000 00     - cmp byte ptr [rsi+0000298E],00
eldenring.exe+708AE4 - 74 31                 - je eldenring.exe+708B17
eldenring.exe+708AE6 - 41 0F28 CD            - movaps xmm1,xmm13
eldenring.exe+708AEA - 48 8B CE              - mov rcx,rsi
eldenring.exe+708AED - 80 BE 8F290000 00     - cmp byte ptr [rsi+0000298F],00
eldenring.exe+708AF4 - 74 07                 - je eldenring.exe+708AFD
eldenring.exe+708AF6 - E8 B50D0000           - call eldenring.exe+7098B0
eldenring.exe+708AFB - EB 05                 - jmp eldenring.exe+708B02
eldenring.exe+708AFD - E8 1E120000           - call eldenring.exe+709D20
eldenring.exe+708B02 - E8 D9CB6F00           - call eldenring.exe+E056E0

E8 ??????FF 80 BE ????0000 00 74 ?? 41 0F28 CD 48 8B CE 80 BE ????0000 00 74 07 E8 ??????00 EB 05 E8 ??????00 E8 ??????00
*/
std::vector<UINT16> PATTERN_PLAYER_AIM = { 0xE8, MASK, MASK, MASK, 0xFF, 0x80, 0xBE, MASK, MASK, 0x00, 0x00, 0x00, 0x74, MASK, 0x41, 0x0F, 0x28, 0xCD, 0x48, 0x8B, 0xCE, 0x80, 0xBE, MASK, MASK, 0x00, 0x00, 0x00, 0x74, 0x07, 0xE8, MASK, MASK, MASK, 0x00, 0xEB, 0x05, 0xE8, MASK, MASK, MASK, 0x00, 0xE8, MASK, MASK, MASK, 0x00 };

/*
eldenring.exe+40863A - E8 B193FEFF           - call eldenring.exe+3F19F0
eldenring.exe+40863F - 84 C0                 - test al,al
eldenring.exe+408641 - 75 B8                 - jne eldenring.exe+4085FB
eldenring.exe+408643 - 49 8B CE              - mov rcx,r14
eldenring.exe+408646 - E8 85190300           - call eldenring.exe+439FD0
eldenring.exe+40864B - 48 8B C8              - mov rcx,rax
eldenring.exe+40864E - E8 7D9BFEFF           - call eldenring.exe+3F21D0
eldenring.exe+408653 - 84 C0                 - test al,al
eldenring.exe+408655 - 75 A4                 - jne eldenring.exe+4085FB

E8 ??????FF 84 C0 75 ?? 49 8B CE E8 ??????00 48 8B C8 E8 ??????FF 84 C0 75 ??
*/
std::vector<UINT16> PATTERN_LOCKON_STATE = { 0xE8, MASK, MASK, MASK, 0xFF, 0x84, 0xC0, 0x75, MASK, 0x49, 0x8B, 0xCE, 0xE8, MASK, MASK, MASK, 0x00, 0x48, 0x8B, 0xC8, 0xE8, MASK, MASK, MASK, 0xFF, 0x84, 0xC0, 0x75, MASK };

/*
eldenring.exe+408360 - 40 53                 - push rbx
eldenring.exe+408362 - 48 83 EC 20           - sub rsp,20
eldenring.exe+408366 - 48 8B D9              - mov rbx,rcx
eldenring.exe+408369 - E8 52020000           - call eldenring.exe+4085C0
eldenring.exe+40836E - 48 89 43 70           - mov [rbx+70],rax
eldenring.exe+408372 - 48 83 C4 20           - add rsp,20
eldenring.exe+408376 - 5B                    - pop rbx
eldenring.exe+408377 - C3                    - ret

40 53 48 83 EC 20 48 8B D9 E8 ??????00 48 89 43 ?? 48 83 C4 20 5B C3
*/
std::vector<UINT16> PATTERN_SPELL_AIM = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0xE8, MASK, MASK, MASK, 0x00, 0x48, 0x89, 0x43, MASK, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3 };

/*
eldenring.exe+3D9C69 - 74 0F                 - je eldenring.exe+3D9C7A
eldenring.exe+3D9C6B - F3 0F10 4E 08         - movss xmm1,[rsi+08]
eldenring.exe+3D9C70 - 48 8B CF              - mov rcx,rdi
eldenring.exe+3D9C73 - E8 D8040000           - call eldenring.exe+3DA150
eldenring.exe+3D9C78 - EB 2A                 - jmp eldenring.exe+3D9CA4

74 0F F3 0F10 4E 08 48 8B CF E8 ??????00 EB 2A
*/
std::vector<UINT16> PATTERN_FREE_MOVEMENT = { 0x74, 0x0F, 0xF3, 0x0F, 0x10, 0x4E, 0x08, 0x48, 0x8B, 0xCF, 0xE8, MASK, MASK, MASK, 0x00, 0xEB, 0x2A };

/*
eldenring.exe+3DA694 - E8 A717FFFF           - call eldenring.exe+3CBE40
eldenring.exe+3DA699 - 45 0F57 CB            - xorps xmm9,xmm11
eldenring.exe+3DA69D - 41 0F28 C1            - movaps xmm0,xmm9
eldenring.exe+3DA6A1 - F3 0F59 05 97EF1704   - mulss xmm0,[eldenring.exe+4559640]
eldenring.exe+3DA6A9 - F3 0F11 45 A0         - movss [rbp-60],xmm0

E8 ??????FF 45 0F57 CB 41 0F28 C1 F3 0F59 05 ???????? F3 0F11 45 A0
*/
std::vector<UINT16> PATTERN_SET_ROTATION = { 0xE8, MASK, MASK, MASK, 0xFF, 0x45, 0x0F, 0x57, 0xCB, 0x41, 0x0F, 0x28, 0xC1, 0xF3, 0x0F, 0x59, 0x05, MASK, MASK, MASK, MASK, 0xF3, 0x0F, 0x11, 0x45, 0xA0 };

float Frametime;
glm::mat4 CameraMatrix;
glm::vec4 PivotPosUninterp;
glm::vec4 PrevPivotPos;
glm::vec3 PivotVelocity;
bool bIsSprinting;
bool bIsOnTorrent;

struct TPlayerInput
{
    char _other0[0x198];
    bool bIsIdle;
    bool bCanRotate; //?
    char _other1[0x1B6];
    // +350
    glm::vec4 InputDir; // relative to player char
    char _other2[0x40];
    // +3A0
    float InputScale; // == 2.0 when sprinting
    float InputAngleDeg;
};

enum EMovement : UINT
{
    NORMAL = 0x2D
};

struct TPlayerAim
{
    char _other0[0x58];
    TPlayerInput* pInput; // +58 (8B)
    char _other1[0x69]; // nice
    bool bLookAt; // +C9 (1B)
    char _other2[0x6];
    glm::vec4 Target; // +D0 (16B)
    char _other3[0x454];
    UINT MovementMode; // +534
};

bool bHasLockon;
bool bIsIdle;

void (*tram_PlayerAim)(PVOID, bool);
void hk_PLayerAim(TPlayerAim* pAimData, bool bHasLockon)
{
    bIsSprinting = pAimData->pInput->InputScale > 1.f;
    ::bHasLockon = bHasLockon;
    if (bHasLockon)
    {
        tram_PlayerAim(pAimData, bHasLockon);
        return;
    }

    bIsIdle = pAimData->MovementMode == EMovement::NORMAL && pAimData->pInput->InputScale == 0.f;
    bool bChangeRotation = Config.bAlwaysTurn ? true : !bIsIdle;

    glm::vec3 CamFwd = CameraMatrix * glm::vec4(0, 0, 1, 0);
    pAimData->Target = glm::vec4(PivotPosUninterp.xyz + CamFwd * 100.f + glm::vec3(0, 10.f, 0), 1.f);

    tram_PlayerAim(pAimData, bChangeRotation);
}

void (*tram_CameraTick)(LPVOID, float, LPVOID, LPVOID);
void hk_CameraTick(LPVOID rcx, float xmm1, LPVOID r8, LPVOID r9)
{
    tram_CameraTick(rcx, xmm1, r8, r9);

    PrevPivotPos = PivotPosUninterp;
    memcpy(glm::value_ptr(CameraMatrix), LPVOID(UINT_PTR(rcx) + 0x10), sizeof(float) * 16);
    memcpy(glm::value_ptr(PivotPosUninterp), LPVOID(UINT_PTR(rcx) + 0xB0), sizeof(float) * 4);

    PivotVelocity = (PivotPosUninterp.xyz - PrevPivotPos.xyz) * (1.f / xmm1);
}

bool bCallingSpellAim;

// affects directional rolls/jumps, look-at animations
bool (*tram_GetLockonState)(PVOID);
bool hk_GetLockonState(PVOID p)
{
    if (bCallingSpellAim)
    {
        return bHasLockon;
    }
    return true;
    //return tram_GetLockonState(p);
}

PVOID (*tram_SpellAim)(PVOID);
PVOID hk_SpellAim(PVOID p)
{
    bCallingSpellAim = true;
    PVOID pOut = tram_SpellAim(p);
    bCallingSpellAim = false;
    return pOut;
}

bool bCallingMovement;

void (*tram_FreeMovement)(LPVOID, float);
void hk_FreeMovement(LPVOID p, float f)
{
    bCallingMovement = true;
    bIsOnTorrent = *reinterpret_cast<LPBYTE>(UINT_PTR(p) + 0x1DC);
    tram_FreeMovement(p, f);
    bCallingMovement = false;
}

void (*tram_SetRotation)(LPVOID, LPVOID);
void hk_SetRotation(LPVOID p0, LPVOID p1)
{
    bool bSkipRotation = false;
    if (bCallingMovement && bIsIdle)
    {
        bSkipRotation = !(bIsSprinting || bIsOnTorrent);
    }
    if (!bSkipRotation)
    {
        tram_SetRotation(p0, p1);
    }
}

LPVOID CreateHook(std::string id, std::vector<UINT16> pattern, LPVOID pDetour, LPVOID* pTrampoline, int offset = 0)
{
    MH_STATUS mh;
    UINT_PTR pScan = ModUtils::SigScan(pattern, id);

    if (pScan)
    {
        UINT_PTR Call = pScan + offset;
        INT32 RelAddr = *reinterpret_cast<INT32*>(Call + 1);
        LPVOID pTarget = reinterpret_cast<LPVOID>(Call + 5 + RelAddr);

        mh = MH_CreateHook(pTarget, pDetour, pTrampoline);
        ModUtils::Log("Creating hook %s %s", id.c_str(), MH_StatusToString(mh));
        mh = MH_EnableHook(pTarget);
        ModUtils::Log("%s", MH_StatusToString(mh));
    }
    return reinterpret_cast<LPVOID>(pScan);
}

DWORD WINAPI MainThread(LPVOID lpParams)
{
    MH_STATUS mh;

    mh = MH_Initialize();
    ModUtils::Log("%s", MH_StatusToString(mh));

    CreateHook("CameraTick", PATTERN_CAMERA_TICK, &hk_CameraTick, (LPVOID*)&tram_CameraTick, 0xFC - 0xD8);
    CreateHook("PlayerAim", PATTERN_PLAYER_AIM, &hk_PLayerAim, (LPVOID*)&tram_PlayerAim);
    CreateHook("LockonState", PATTERN_LOCKON_STATE, &hk_GetLockonState, (LPVOID*)&tram_GetLockonState);
    CreateHook("SpellAim", PATTERN_SPELL_AIM, &hk_SpellAim, (LPVOID*)&tram_SpellAim, 9);
    CreateHook("FreeMovement", PATTERN_FREE_MOVEMENT, &hk_FreeMovement, (LPVOID*)&tram_FreeMovement, 10);
    CreateHook("SetRotation", PATTERN_SET_ROTATION, &hk_SetRotation, (LPVOID*)&tram_SetRotation);

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
        //DisableThreadLibraryCalls(hModule);
        LoadLibraryA("mods\\bin\\MinHook.x64.dll");
        CreateThread(0, 0, &MainThread, 0, 0, 0);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

