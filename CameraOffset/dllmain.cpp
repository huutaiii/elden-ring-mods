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
            MaxDistance = MemToGLM(baseAddr + 0x1B0).y;
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
    __m128 TargetOffset;

    void GetFrametime();
    void GetCameraData();
    void SetCameraOffset();
    void SetCollisionOffset();
}

// Called by asm hook to make sure the pointer is always valid
extern "C" void ReadCameraData()
{
    CameraData = FCameraData(CamBaseAddr);
}

extern "C" void CalcCameraOffset()
{
    glm::mat4 rotation = glm::rotateNormalizedAxis(glm::mat4(1), CameraData.Rotation.y, glm::vec3(0, 1, 0));
    float DistanceScale = saturate(glm::distance(CameraData.LocPivotInterp, CameraData.LocFinalInterp) / CameraData.MaxDistance);
    glm::vec3 cameraOffset = rotation * Config.Offset;
    glm::vec3 collisionOffset = rotation * Config.Offset;
    glm::vec3 cameraOffsetInterp = InterpToV(glm::vec3(XMMtoGLM(CameraOffset)), cameraOffset, Config.OffsetInterpSpeed, Frametime);
    glm::vec3 collisionOffsetInterp = InterpToV(glm::vec3(XMMtoGLM(CollisionOffset)), collisionOffset, Config.OffsetInterpSpeed, Frametime);
    CameraOffset = GLMtoXMM(cameraOffsetInterp);
    CollisionOffset = GLMtoXMM(cameraOffsetInterp);
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
std::vector<uint16_t> PATTERN_CAMERA_DATA = { 0x48, 0x8B, 0xf1, MASK, 0x85, MASK, 0x75, MASK, 0x48, 0x8D, MASK, MASK, MASK, MASK, MASK, 0xE8, MASK, MASK, MASK, MASK, 0x4C, 0x8B, MASK };
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
*/
std::vector<uint16_t> PATTERN_CAMERA_OFFSET = { 0x0F, 0x29, 0x87, 0x40, 0x04, 0x00, 0x00, 0x0F, 0x29, 0x8F, 0x50, 0x04, 0x00, 0x00, 0x48, 0x83, 0x3D, 0x09, 0xC4, 0x89, 0x03, 0x00, 0x48, 0x89, 0xAC, 0x24, 0x20, 0x01, 0x00, 0x00, 0x75, 0x27 };
UHookRelativeIntermediate HookSetCameraOffset(
    "HookSetCameraOffset",
    PATTERN_CAMERA_OFFSET,
    7,
    &SetCameraOffset
);

/*
eldenring.exe.text+BFC5F0 - 49 89 43 A0           - mov [r11-60],rax
eldenring.exe.text+BFC5F4 - F3 0F11 44 24 20      - movss [rsp+20],xmm0
eldenring.exe.text+BFC5FA - 0F29 4C 24 50         - movaps [rsp+50],xmm1
eldenring.exe.text+BFC5FF - E8 9CFBFFFF           - call eldenring.exe.text+BFC1A0
eldenring.exe.text+BFC604 - 84 C0                 - test al,al
*/
std::vector<uint16_t> PATTERN_COLLISON_OFFSET = { 0x49, 0x89, 0x43, 0xA0, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x20, 0x0F, 0x29, 0x4C, 0x24, 0x50, 0xE8, 0x9C, 0xFB, 0xFF, 0xFF, 0x84, 0xC0 };
UHookRelativeIntermediate HookSetCollisionOffset(
    "HookSetCollisionOffset",
    PATTERN_COLLISON_OFFSET,
    6,
    &SetCollisionOffset,
    4
);

/*
eldenring.exe.text+3B6CDB - 0F29 44 24 50         - movaps [rsp+50],xmm0						; collision trace end
eldenring.exe.text+3B6CE0 - 48 8B C8              - mov rcx,rax
eldenring.exe.text+3B6CE3 - F3 0F10 83 C0010000   - movss xmm0,[rbx+000001C0]
eldenring.exe.text+3B6CEB - F3 0F11 44 24 20      - movss [rsp+20],xmm0
eldenring.exe.text+3B6CF1 - E8 BA588400           - call eldenring.exe.text+BFC5B0				; checks collision and interpolate?
*/
std::vector<uint16_t> PATTERN_COLLISION_ALT = { 0x0F, 0x29, 0x44, 0x24, 0x50, 0x48, 0x8B, 0xC8, 0xF3, 0x0F, 0x10, 0x83, 0xC0, 0x01, 0x00, 0x00, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x20, 0xE8, 0xBA, 0x58, 0x84, 0x00 };
UHookRelativeIntermediate HookSetCollisionOffsetAlt(
    "HookSetCollisionOffsetAlt",
    PATTERN_COLLISION_ALT,
    5,
    &SetCollisionOffset
);

/*
eldenring.exe.text+D89AD8 - F3 0F11 49 04         - movss [rcx+04],xmm1
eldenring.exe.text+D89ADD - F3 0F58 09            - addss xmm1,[rcx]
eldenring.exe.text+D89AE1 - 0F2F C8               - comiss xmm1,xmm0
eldenring.exe.text+D89AE4 - F3 0F11 09            - movss [rcx],xmm1
eldenring.exe.text+D89AE8 - 72 15                 - jb eldenring.exe.text+D89AFF
eldenring.exe.text+D89AEA - F3 0F10 15 DEBCD801   - movss xmm2,[eldenring.exe.rdata+2177D0]
*/
std::vector<uint16_t> PATTERN_FRAMETIME = { 0xF3, 0x0F, 0x11, 0x49, 0x04, 0xF3, 0x0F, 0x58, 0x09, 0x0F, 0x2F, 0xC8, 0xF3, 0x0F, 0x11, 0x09, 0x72, 0x15, 0xF3, 0x0F, 0x10, 0x15, 0xDE, 0xBC, 0xD8, 0x01 };
UHookRelativeIntermediate HookGetFrametime(
    "HookGetFrametime",
    PATTERN_FRAMETIME,
    5,
    &GetFrametime
);

#pragma warning(suppress: 4100)
DWORD WINAPI MainThread(LPVOID lpParam)
{
    HookGetFrametime.Enable();
    HookGetCameraData.Enable();
    HookSetCameraOffset.Enable();
    HookSetCollisionOffsetAlt.Enable();

    {
        uintptr_t addr = ModUtils::SigScan({ 0x0F, 0x84, 0xC3, 0x00, 0x00, 0x00, 0x0F, 0x28, 0x65, 0xD0, 0x0F, 0x28, 0xD4, 0x0F, 0x59, 0xD4, 0x0F, 0x28, 0xCA, 0x0F, 0xC6, 0xCA, 0x66, 0xF3, 0x0F, 0x58, 0xD1 });
        if (addr)
        {
            *reinterpret_cast<char*>(addr) = 0x90;
            *reinterpret_cast<char*>(addr+1) = 0xE9;
        }
    }

    while (true)
    {
        if (ModUtils::CheckHotkey(0x70))
        {
            HookSetCameraOffset.Toggle();
            //printf("%p\n", CamBaseAddr);
            //printf("%s\n", glm::to_string(CameraData.DirForward).c_str());
            //printf("%s\n", glm::to_string(CameraData.LocFinal).c_str());
            //printf("%s\n", glm::to_string(CameraData.LocPivot).c_str());
            //printf("%s\n", glm::to_string(CameraData.LocPivotInterp).c_str());
            //printf("%s\n", glm::to_string(CameraData.LocTarget).c_str());
            //printf("%f %f\n", CameraData.MaxDistance, glm::length(CameraData.LocFinal - CameraData.LocPivotInterp));
        }
        if (ModUtils::CheckHotkey(0x71))
        {
            HookSetCollisionOffset.Toggle();
        }
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

