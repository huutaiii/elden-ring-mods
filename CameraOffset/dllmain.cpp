#pragma warning(push,0)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xmmintrin.h>
#include <vector>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_normalized_axis.hpp>
#pragma warning(pop)

#include <ModUtils.h>
#include <HookUtils.h>
#include <MathUtils.h>

using ModUtils::MASK;

struct FModConfig
{
    glm::vec4 Offset = {};
    float OffsetInterpSpeed = 0;
};
FModConfig Config = { {1.f, 0.f, 0.f, 1.f}, 8.f };

struct FCameraData
{
    // Pivot position (uninterpolated)
    glm::vec4 LocPivot = {};

    // Pivot position (interpolated)
    glm::vec4 LocPivotInterp = {};

    // Final camera position, with offset(?) and collision
    glm::vec4 LocFinal = {};

    // Doesn't always track towards LocFinal
    glm::vec4 LocFinalInterp = {};

    // .x = Pitch, .y = Yaw
    glm::vec2 Rotation = {};

    // Target position (lock-on)
    glm::vec4 LocTarget = {};

    float MaxDistance = 0;

    FCameraData(LPVOID BaseAddr = nullptr)
    {
        if (BaseAddr)
        {
            char* baseAddr = (char*)BaseAddr;

            LocPivot = MemToGLM(baseAddr + 0xB0);
            LocPivotInterp = MemToGLM(baseAddr + 0xC0);
            LocFinal = MemToGLM(baseAddr + 0x100);
            LocFinalInterp = MemToGLM(baseAddr + 0x110);
            Rotation = MemToGLM(baseAddr + 0x150);
            LocTarget = MemToGLM(baseAddr + 0x2F0);
            // .x = distance with collision but lower?, .y and .z each has a slightly different interpolation speed, no collision
            MaxDistance = MemToGLM(baseAddr + 0x1B0).z;
        }
    }

};
FCameraData CameraData;

extern "C"
{
    // in
    LPVOID CamBaseAddr;
    float Frametime;

    // out
    __m128 CameraOffset;
    __m128 CollisionOffset;
    __m128 RetractCollisionOffset;
    __m128 TargetOffset;

    void GetFrametime();
    void GetCameraData();
    void SetCameraOffset();
    void SetCollisionOffset();
    void SetCollisionOffsetAlt();
    void SetCollisionOffsetAlt1();
    void AdjustCollision();
    void AdjustCollision01();
    void AdjustCollision1();
}

// Called by asm hook to make sure the pointer is always valid
extern "C" void ReadCameraData()
{
    CameraData = FCameraData(CamBaseAddr);
}

extern "C" void CalcCameraOffset()
{
    glm::mat4 rotation = glm::rotateNormalizedAxis(glm::mat4(1), CameraData.Rotation.y, glm::vec3(0, 1, 0));
    float RetractAlpha = saturate(glm::distance(CameraData.LocPivotInterp, CameraData.LocFinalInterp) / CameraData.MaxDistance);
    glm::vec3 cameraOffset = rotation * Config.Offset;
    glm::vec3 collisionOffset = rotation * Config.Offset;
    glm::vec3 cameraOffsetInterp = InterpToV(glm::vec3(XMMtoGLM(CameraOffset)), cameraOffset, Config.OffsetInterpSpeed, Frametime);
    glm::vec3 collisionOffsetInterp = InterpToV(glm::vec3(XMMtoGLM(CollisionOffset)), collisionOffset, Config.OffsetInterpSpeed, Frametime);
    CameraOffset = GLMtoXMM(cameraOffsetInterp);
    CollisionOffset = GLMtoXMM(collisionOffsetInterp);
    RetractCollisionOffset = GLMtoXMM(collisionOffsetInterp * RetractAlpha);
}

/*
    eldenring.exe.text+3B1CCF - 44 0F28 F9            - movaps xmm15,xmm1
    eldenring.exe.text+3B1CD3 - 48 8B F1              - mov rsi,rcx
    eldenring.exe.text+3B1CD6 - 4D 85 F6              - test r14,r14
    eldenring.exe.text+3B1CD9 - 75 2E                 - jne eldenring.exe.text+3B1D09
    eldenring.exe.text+3B1CDB - 48 8D 0D 1FDC8803     - lea rcx,[eldenring.exe.data+229901]
    eldenring.exe.text+3B1CE2 - E8 79A2A701           - call eldenring.exe.text+1E2BF60
    eldenring.exe.text+3B1CE7 - 4C 8B C8              - mov r9,rax
*/
std::vector<uint16_t> PATTERN_CAMERA_DATA = { 0x48, 0x8B, 0xf1, 0x4d, 0x85, 0xf6, 0x75, MASK, 0x48, 0x8D, 0x0d, MASK, MASK, MASK, MASK, 0xE8, MASK, MASK, MASK, MASK, 0x4C, 0x8B, 0xc8 };
// Read the camera data struct from the game
UHookRelativeIntermediate HookGetCameraData(
    "HookGetCameraData",
    PATTERN_CAMERA_DATA,
    7,
    &GetCameraData,
    -4
);

/*
eldenring.exe.text+3B27B9 - 0F29 87 40040000      - movaps [rdi+00000440],xmm0
eldenring.exe.text+3B27C0 - 0F29 8F 50040000      - movaps [rdi+00000450],xmm1
eldenring.exe.text+3B27C7 - 48 83 3D 09C48903 00  - cmp qword ptr [eldenring.exe+3C4FBD8],00
eldenring.exe.text+3B27CF - 48 89 AC 24 20010000  - mov [rsp+00000120],rbp
eldenring.exe.text+3B27D7 - 75 27                 - jne eldenring.exe.text+3B2800

0F29??????????0F29??????????48833D??????????4889AC??????????75??
*/
std::vector<uint16_t> PATTERN_CAMERA_OFFSET = { 0x0F, 0x29, MASK, MASK, MASK, MASK, MASK, 0x0F, 0x29, MASK, MASK, MASK, MASK, MASK, 0x48, 0x83, 0x3D, MASK, MASK, MASK, MASK, MASK, 0x48, 0x89, 0xAC, MASK, MASK, MASK, MASK, MASK, 0x75, MASK };
// offsets the camera's final position
UHookRelativeIntermediate HookSetCameraOffset(
    "HookSetCameraOffset",
    PATTERN_CAMERA_OFFSET,
    7,
    &SetCameraOffset
);

/*
eldenring.exe+C3F4B3 - 48 8D 4D 10           - lea rcx,[rbp+10]
eldenring.exe+C3F4B7 - E8 C4F71800           - call eldenring.exe+DCEC80
eldenring.exe+C3F4BC - 41 0F28 45 00         - movaps xmm0,[r13+00]
eldenring.exe+C3F4C1 - 48 8B 44 24 38        - mov rax,[rsp+38]
eldenring.exe+C3F4C6 - 0F58 00               - addps xmm0,[rax]                             ; [rax] = local camera position
eldenring.exe+C3F4C9 - 0F29 44 24 40         - movaps [rsp+40],xmm0
eldenring.exe+C3F4CE - 48 8D 54 24 40        - lea rdx,[rsp+40]
eldenring.exe+C3F4D3 - 48 8D 4D 00           - lea rcx,[rbp+00]
eldenring.exe+C3F4D7 - E8 A4F71800           - call eldenring.exe+DCEC80
*/
std::vector<uint16_t> PATTERN_COLLISON_OFFSET = { 0x48, 0x8B, 0x44, 0x24, 0x38, 0x0F, 0x58, 0x00, 0x0F, 0x29, 0x44, 0x24, 0x40, 0x48, 0x8D, 0x54, 0x24, 0x40, 0x48, 0x8D, 0x4D, 0x00, 0xE8, 0xA4, 0xF7, 0x18, 0x00 };
UHookRelativeIntermediate HookSetCollisionOffset(
    "HookSetCollisionOffset",
    PATTERN_COLLISON_OFFSET,
    5,
    &SetCollisionOffset
);

/*
eldenring.exe+3B8EF6 - 4C 8D 44 24 60        - lea r8,[rsp+60]
eldenring.exe+3B8EFB - 0F29 44 24 50         - movaps [rsp+50],xmm0
eldenring.exe+3B8F00 - 48 8B C8              - mov rcx,rax
eldenring.exe+3B8F03 - F3 0F10 83 C0010000   - movss xmm0,[rbx+000001C0]
eldenring.exe+3B8F0B - F3 0F11 44 24 20      - movss [rsp+20],xmm0
eldenring.exe+3B8F11 - E8 FA688800           - call eldenring.exe+C3F810
eldenring.exe+3B8F16 - 84 C0                 - test al,al
eldenring.exe+3B8F18 - 74 5C                 - je eldenring.exe+3B8F76
*/
std::vector<uint16_t> PATTERN_COLLISION_ALT = { 0x4C, 0x8D, 0x44, 0x24, 0x60, 0x0F, 0x29, 0x44, 0x24, 0x50, 0x48, 0x8B, 0xC8, 0xF3, 0x0F, 0x10, 0x83, 0xC0, 0x01, 0x00, 0x00, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x20, 0xE8, 0xFA, 0x68, 0x88, 0x00, 0x84, 0xC0, 0x74, 0x5C };
// Disables one of the collision checks
UHookRelativeIntermediate HookSetCollisionOffsetAlt(
    "HookSetCollisionOffsetAlt",
    PATTERN_COLLISION_ALT,
    5,
    &SetCollisionOffsetAlt
);

/*
eldenring.exe+3B583D - BA 5B000000           - mov edx,0000005B
eldenring.exe+3B5842 - 48 8B CF              - mov rcx,rdi
eldenring.exe+3B5845 - 0F29 75 D0            - movaps [rbp-30],xmm6
eldenring.exe+3B5849 - F3 0F11 44 24 20      - movss [rsp+20],xmm0
eldenring.exe+3B584F - E8 BC9F8800           - call eldenring.exe+C3F810
eldenring.exe+3B5854 - 84 C0                 - test al,al
eldenring.exe+3B5856 - 0F84 C3000000         - je eldenring.exe+3B591F
*/
std::vector<uint16_t> PATTERN_COLLISION_ALT1 = { 0xBA, 0x5B, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCF, 0x0F, 0x29, 0x75, 0xD0, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x20, 0xE8, 0xBC, 0x9F, 0x88, 0x00, 0x84, 0xC0, 0x0F, 0x84, 0xC3, 0x00, 0x00, 0x00 };
UHookRelativeIntermediate HookSetCollisionOffsetAlt1(
    "HookCollisionOffsetAlt1",
    PATTERN_COLLISION_ALT1,
    5,
    &SetCollisionOffsetAlt1
);

/*
eldenring.exe+3B5911 - 0F59 D4               - mulps xmm2,xmm4
eldenring.exe+3B5914 - 0F59 D0               - mulps xmm2,xmm0
eldenring.exe+3B5917 - 0F58 55 E0            - addps xmm2,[rbp-20]
eldenring.exe+3B591B - 0F29 53 40            - movaps [rbx+40],xmm2
eldenring.exe+3B591F - 48 8B 4D 00           - mov rcx,[rbp+00]
eldenring.exe+3B5923 - 48 33 CC              - xor rcx,rsp
eldenring.exe+3B5926 - E8 B5BA0F02           - call eldenring.exe+24B13E0
*/
std::vector<uint16_t> PATTERN_COLLISION_ADJUST1 = { 0x0F, 0x59, 0xD4, 0x0F, 0x59, 0xD0, 0x0F, 0x58, 0x55, 0xE0, 0x0F, 0x29, 0x53, 0x40, 0x48, 0x8B, 0x4D, 0x00, 0x48, 0x33, 0xCC, 0xE8, 0xB5, 0xBA, 0x0F, 0x02 };
// the collision function sets a location for the camera when the collision trace hits
// which is unintentionally offset
// this reverts the offset
UHookRelativeIntermediate HookCollisionAdjust1(
    "HookCollisionAdjust1",
    PATTERN_COLLISION_ADJUST1,
    6,
    &AdjustCollision1
);

/*
eldenring.exe+3B8F6A - 0F58 54 24 60         - addps xmm2,[rsp+60]
eldenring.exe+3B8F6F - 0F29 93 00010000      - movaps [rbx+00000100],xmm2
eldenring.exe+3B8F76 - 48 8B 8C 24 80000000  - mov rcx,[rsp+00000080]
eldenring.exe+3B8F7E - 48 33 CC              - xor rcx,rsp
eldenring.exe+3B8F81 - E8 5A840F02           - call eldenring.exe+24B13E0
eldenring.exe+3B8F86 - 0F28 B4 24 90000000   - movaps xmm6,[rsp+00000090]
eldenring.exe+3B8F8E - 48 81 C4 A0000000     - add rsp,000000A0
eldenring.exe+3B8F95 - 5B                    - pop rbx
*/
std::vector<uint16_t> PATTERN_COLLISION_ADJUST = { 0x0F, 0x58, 0x54, 0x24, 0x60, 0x0F, 0x29, 0x93, 0x00, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x8C, 0x24, 0x80, 0x00, 0x00, 0x00, 0x48, 0x33, 0xCC, 0xE8, 0x5A, 0x84, 0x0F, 0x02, 0x0F, 0x28, 0xB4, 0x24, 0x90, 0x00, 0x00, 0x00, 0x48, 0x81, 0xC4, 0xA0, 0x00, 0x00, 0x00, 0x5B };
UHookRelativeIntermediate HookCollisionAdjust(
    "HookCollisionAdjust",
    PATTERN_COLLISION_ADJUST,
    5,
    &AdjustCollision
);

/*
eldenring.exe+3B8F1A - 0F28 5C 24 50         - movaps xmm3,[rsp+50]
eldenring.exe+3B8F1F - 0F28 D3               - movaps xmm2,xmm3
eldenring.exe+3B8F22 - 0F59 D3               - mulps xmm2,xmm3
eldenring.exe+3B8F25 - 0F28 CA               - movaps xmm1,xmm2
eldenring.exe+3B8F28 - 0FC6 CA 66            - shufps xmm1,xmm2,66
eldenring.exe+3B8F2C - F3 0F58 D1            - addss xmm2,xmm1
eldenring.exe+3B8F30 - 0F28 C1               - movaps xmm0,xmm1
eldenring.exe+3B8F33 - 0FC6 C1 55            - shufps xmm0,xmm1,55
*/
std::vector<uint16_t> PATTERN_COLLISION_ADJUST01 = { 0x0F, 0x28, 0x5C, 0x24, 0x50, 0x0F, 0x28, 0xD3, 0x0F, 0x59, 0xD3, 0x0F, 0x28, 0xCA, 0x0F, 0xC6, 0xCA, 0x66, 0xF3, 0x0F, 0x58, 0xD1, 0x0F, 0x28, 0xC1, 0x0F, 0xC6, 0xC1, 0x55 };
UHookRelativeIntermediate HookCollisionAdjust01(
    "HookCollisionAdjust01",
    PATTERN_COLLISION_ADJUST01,
    5,
    &AdjustCollision01
);

/*
eldenring.exe.text+D89AD8 - F3 0F11 49 04         - movss [rcx+04],xmm1
eldenring.exe.text+D89ADD - F3 0F58 09            - addss xmm1,[rcx]
eldenring.exe.text+D89AE1 - 0F2F C8               - comiss xmm1,xmm0
eldenring.exe.text+D89AE4 - F3 0F11 09            - movss [rcx],xmm1
eldenring.exe.text+D89AE8 - 72 15                 - jb eldenring.exe.text+D89AFF
eldenring.exe.text+D89AEA - F3 0F10 15 DEBCD801   - movss xmm2,[eldenring.exe.rdata+2177D0]

F30F11????F30F58??0F2F??F30F11??72??F30F10??????????
*/
std::vector<uint16_t> PATTERN_FRAMETIME = { 0xF3, 0x0F, 0x11, MASK, MASK, 0xF3, 0x0F, 0x58, MASK, 0x0F, 0x2F, MASK, 0xF3, 0x0F, 0x11, MASK, 0x72, MASK, 0xF3, 0x0F, 0x10, MASK, MASK, MASK, MASK, MASK };
// gets the time since the previous frame
UHookRelativeIntermediate HookGetFrametime(
    "HookGetFrametime",
    PATTERN_FRAMETIME,
    5,
    &GetFrametime
);

#pragma warning(suppress: 4100)
DWORD WINAPI MainThread(LPVOID lpParam)
{
    std::vector<UHookRelativeIntermediate*> hooks {
        &HookGetFrametime,
        &HookGetCameraData,
        &HookSetCameraOffset,
        &HookSetCollisionOffsetAlt,
        &HookSetCollisionOffsetAlt1,
        //&HookCollisionAdjust,
        //&HookCollisionAdjust01,
        &HookCollisionAdjust1,
    };
    for (UHookRelativeIntermediate* hook : hooks)
    {
        hook->Enable();
    }
    /*HookGetFrametime.Enable();
    HookGetCameraData.Enable();
    HookSetCameraOffset.Enable();
    HookSetCollisionOffsetAlt.Enable();
    HookSetCollisionOffsetAlt1.Enable();
    HookCollisionAdjust.Enable();
    HookCollisionAdjust01.Enable();
    HookCollisionAdjust1.Enable();*/

    // disables one of the collision checks?
    {
        uintptr_t addr = ModUtils::SigScan({ 0x0F, 0x84, 0xC3, 0x00, 0x00, 0x00, 0x0F, 0x28, 0x65, 0xD0, 0x0F, 0x28, 0xD4, 0x0F, 0x59, 0xD4, 0x0F, 0x28, 0xCA, 0x0F, 0xC6, 0xCA, 0x66, 0xF3, 0x0F, 0x58, 0xD1 });
        //if (addr)
        //{
        //    *reinterpret_cast<char*>(addr) = 0x90;
        //    *reinterpret_cast<char*>(addr+1) = 0xE9;
        //}
    }

    while (true)
    {
        if (ModUtils::CheckHotkey(0x70))
        {
            printf("CamBaseAddr = %p\n", CamBaseAddr);
            //printf("%s\n", glm::to_string(CameraData.DirForward).c_str());
            printf("LocFinal = %s\n", glm::to_string(CameraData.LocFinal).c_str());
            printf("LocPivot = %s\n", glm::to_string(CameraData.LocPivot).c_str());
            printf("LocPivotInterp = %s\n", glm::to_string(CameraData.LocPivotInterp).c_str());
            printf("LocTarget = %s\n", glm::to_string(CameraData.LocTarget).c_str());
            printf("Rotation = %s\n", glm::to_string(CameraData.Rotation).c_str());
            printf("%f %f\n", CameraData.MaxDistance, saturate(glm::distance(CameraData.LocPivotInterp, CameraData.LocFinalInterp) / CameraData.MaxDistance));
        }
        if (ModUtils::CheckHotkey(0x71))
        {
            HookSetCameraOffset.Toggle();
            HookGetFrametime.Toggle();
            HookGetCameraData.Toggle();
        }
        if (ModUtils::CheckHotkey(0x72))
        {
            for (UHookRelativeIntermediate* hook : hooks)
            {
                hook->Toggle();
            }
        }
        //if (ModUtils::CheckHotkey(0x73))
        //{
        //    HookSetCollisionOffsetAlt1.Toggle();
        //}
        Sleep(2);
    }

    ModUtils::CloseLog();
    return 0;
}

#pragma warning(suppress: 4100)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
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

