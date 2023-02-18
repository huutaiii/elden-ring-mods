#pragma warning(push,0)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xmmintrin.h>
#include <vector>
#include <fstream>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_normalized_axis.hpp>
#pragma warning(pop)

#include <ModUtils.h>
#include <HookUtils.h>
#include <MathUtils.h>
#include "Config.h"

using ModUtils::MASK;

constexpr unsigned int ID_NPC_INTERACT = 1000000;
constexpr unsigned int ID_GRACE = 1001000;
constexpr unsigned int ID_GRACE_TABLE = 1001001;

struct FModConfig
{
    glm::vec3 Offset = {};
    glm::vec3 OffsetLockon = {};
    bool bUseSideSwitch = false;
    float OffsetInterpSpeed = 0;
    float SpringbackSpeed = 0;
    float TargetViewOffsetMul = 1.f;
    bool bUseTargetOffset = false;
    bool bUseAutoDisable = false;

    void ReadINI(INIReader ini)
    {
        Offset = ini.GetVec("main", "offset", Offset);
        OffsetLockon = ini.GetVec("main", "offset-lockon", Offset);
        bUseSideSwitch = ini.GetBoolean("main", "dynamic-lockon-offset", bUseSideSwitch);
        OffsetInterpSpeed = ini.GetReal("main", "interpolation-speed", OffsetInterpSpeed);
        SpringbackSpeed = ini.GetReal("main", "collision-springback-speed", SpringbackSpeed);
        TargetViewOffsetMul = ini.GetReal("main", "target-view-offset-multiplier", TargetViewOffsetMul);
        bUseTargetOffset = ini.GetBoolean("main", "use-offset-on-target", bUseTargetOffset);
        bUseAutoDisable = ini.GetBoolean("main", "automatic-disabling", false);

        //ModUtils::Log("offset: %s", glm::to_string(Offset).c_str());
    }

    void ReadFile(std::string path)
    {
        ModUtils::Log("config file path: %s", path.c_str());
        if (!UConfig::CheckConfigFile(path))
        {
            ModUtils::Log("Creating config file");
            std::ofstream file;
            file.open(path);
            file << UConfig::GetDefaultConfig();
            file.close();
        }
        ReadINI(INIReader(path));
    }
};
//FModConfig Config = { {.75f, 0.f, 0.f}, {1.f, 0.f, 0.f}, 10.f, 1.2f };
FModConfig Config;

struct FCameraData
{
    LPVOID BaseAddress = nullptr;

    // Pivot position (uninterpolated)
    glm::vec4 LocPivot = {};

    // Pivot position (interpolated)
    glm::vec4 LocPivotInterp = {};

    // Final camera position, with offset(?) and collision(?)
    glm::vec4 LocFinal = {};

    // Doesn't always track towards LocFinal, no collision(?)
    glm::vec4 LocFinalInterp = {};

    // .x = Pitch, .y = Yaw
    glm::vec2 Rotation = {};

    // Target position (lock-on)
    glm::vec4 LocTarget = {};

    float MaxDistance = 0;

    bool bIsLockedOn = false;

    uint32_t ParamID = 0;

    FCameraData(LPVOID BaseAddr = nullptr) : BaseAddress(BaseAddr)
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
            bIsLockedOn = (*(char*)(baseAddr + 0x49B) & 0x01) != 0;
            ParamID = *(uint32_t*)(baseAddr + 0x460);
        }
    }

};
FCameraData CameraData;

extern "C"
{
    // in
    float Frametime;
    LPVOID CamBaseAddr;
    LPVOID CamSettingsPtr;

    // world space
    __m128 LastCollisionPos;
    bool bLastCollisionHit;
    float LastCollisionDistNormalized;

    uint8_t bIsTalking = 0;
    uint64_t InteractPtr;


    // out
    __m128 CameraOffset;
    __m128 CollisionOffset;
    __m128 RetractCollisionOffset;
    __m128 TargetOffset;
    float MaxDistanceInterp;

    void GetFrametime();
    void GetCameraData();
    void GetCameraSettings();
    void SetCameraOffset();
    void SetCollisionOffset();
    void SetCollisionOffsetAlt();
    void SetCollisionOffsetAlt1();
    void AdjustCollision();
    void AdjustCollision01();
    void AdjustCollision1();
    void ClampMaxDistance();
    void SetTargetOffset();

    void GetTargetViewOffset();
    __m128 TargetViewOffset;
    float TargetViewMaxOffset;
    float TargetViewMaxOffsetMul;

    void GetNPCState();
    void GetInteractState();
    void SetCrit();

    WORD CritAnimElapsed;
}

// Called by asm hook to make sure the pointer is always valid
extern "C" void ReadCameraData()
{
    CameraData = FCameraData(CamBaseAddr);
}

float LockonAlpha = 0.f;
float SideSwitch = 1.f;
float ToggleAlpha = 1.f;
const int EnableOffsetDelay = 2;
int EnableOffsetElapsed = 0;
const WORD CritAnimDelay = 15;

extern "C" void CalcCameraOffset()
{
    {
        static unsigned long FC = 0;
        ++FC;
    }

    TargetViewMaxOffsetMul = Config.TargetViewOffsetMul;

    LockonAlpha = InterpToF(LockonAlpha, CameraData.bIsLockedOn ? 1.f : 0.f, CameraData.bIsLockedOn ? 3.f : 2.f, Frametime);

    if (Config.bUseSideSwitch)
    {
        if (CameraData.bIsLockedOn)
        {

            //printf("%f %s\n", TargetViewMaxOffsetSqr, glm::to_string(XMMtoGLM(TargetViewOffset)).c_str());
            float ViewOffsetY = XMMtoGLM(TargetViewOffset).y;
            int sign = ViewOffsetY > 0.f ? 1 : -1;
            float ViewOffsetClamped = clamp(sign * sqrt(abs(ViewOffsetY / TargetViewMaxOffset)), -1.f, 1.f);
            SideSwitch = InterpToF(SideSwitch, ViewOffsetClamped, 2.5f, Frametime);
            //SideSwitch = InterpToF(SideSwitch, ViewOffsetY > 0.f ? 1.f : -1.f, 1.f, Frametime);
        }
        else
        {
            SideSwitch = InterpToF(SideSwitch, 1.f, 1.f, Frametime);
        }
    }

    static float CritAlpha = 0.f;
    bool bCinematicEffectsEnabled = *(LPBYTE(CamSettingsPtr) + 0x11);
    if (Config.bUseAutoDisable && bCinematicEffectsEnabled)
    {
        // maybe only use this when "Cinematic Effects" is turned on?
        CritAnimElapsed = min(CritAnimElapsed + 1, CritAnimDelay + 1);
        CritAlpha = InterpToF(CritAlpha, CritAnimElapsed < CritAnimDelay ? 1.f : 0.f, CritAnimElapsed < CritAnimDelay ? 5.f : 2.f, Frametime);
    }
    if (Config.bUseAutoDisable)
    {
        //bool bIsResting = CameraData.ParamID == ID_GRACE || CameraData.ParamID == ID_GRACE_TABLE;
        // some NPCs don't trigger the zoom-in so this doesn't always work
        //bool bIsTalking = CameraData.ParamID == ID_NPC_INTERACT;

        //bool bUseOffset = !bIsTalking && !bIsResting;
        // some interactions don't set the pointer, like getting hugs from Fia
        bool bUseOffset = !(InteractPtr > 1 || CameraData.ParamID == ID_NPC_INTERACT);

        // delay re-enabling offset so the camera won't jiggle when going between menus
        if (!bUseOffset)
        {
            EnableOffsetElapsed = 0;
        }
        else
        {
            EnableOffsetElapsed = min(++EnableOffsetElapsed, EnableOffsetDelay + 1);
        }
        ToggleAlpha = InterpToF(ToggleAlpha, EnableOffsetElapsed > EnableOffsetDelay ? 1.f : 0.f, 5.f, Frametime);
    }

    glm::vec3 localMaxOffset = lerp(Config.Offset, Config.OffsetLockon, LockonAlpha) * ToggleAlpha * (1.f - CritAlpha);

    {
        static float GraceInterp = 0.f;
        GraceInterp = InterpToF(GraceInterp, CameraData.ParamID == ID_GRACE ? 1.f : 0.f, 3.f, Frametime);
        localMaxOffset += glm::vec3(-0.5f, 0, 0) * GraceInterp;
    }

    glm::mat4 rotation = glm::rotateNormalizedAxis(glm::mat4(1), CameraData.Rotation.y, glm::vec3(0, 1, 0));
    float RetractAlpha = saturate(glm::distance(CameraData.LocPivotInterp, CameraData.LocFinalInterp) / CameraData.MaxDistance);
    glm::vec3 cameraOffset = rotation * glm::vec4(localMaxOffset * glm::vec3(lerp(1.f, SideSwitch, LockonAlpha), 0, 0), 1);
    glm::vec3 collisionOffset = cameraOffset;

    static float TargetDistAdjust = 1.f;
    if (CameraData.bIsLockedOn)
    {
        float targetDistance = glm::distance(CameraData.LocPivotInterp, CameraData.LocTarget);
        //glm::vec3 targetOffset = cameraOffset * (0.0625f * (-1.f) * pow(targetDistance > 1.f ? targetDistance : pow(targetDistance, 1 / targetDistance), 1.125f));

        // clamp offset when locked on to avoid the camera spinning around
        TargetDistAdjust = pow(1.f - 1 / exp2(max(targetDistance - .1f, 0.f)), 2);

        if (Config.bUseTargetOffset)
        {
            glm::vec3 targetOffset = cameraOffset * lerp(0.f, -.1f, smoothstep(2.f, 5.f, targetDistance)) * targetDistance;
            TargetOffset = GLMtoXMM(targetOffset);
        }
    }
    else
    {
        TargetDistAdjust = InterpToF(TargetDistAdjust, 1.f, 2.f, Frametime);
    }
    cameraOffset *= TargetDistAdjust;
    collisionOffset *= TargetDistAdjust;

    glm::vec3 cameraOffsetInterp = InterpSToV(glm::vec3(XMMtoGLM(CameraOffset)), cameraOffset, Config.OffsetInterpSpeed, Frametime);
    glm::vec3 collisionOffsetInterp = cameraOffsetInterp;
    CollisionOffset = GLMtoXMM(collisionOffsetInterp);
    RetractCollisionOffset = GLMtoXMM(collisionOffsetInterp * RetractAlpha);

    //if (!bLastCollisionHit)
    //{
    //    LastCollisionDistNormalized = InterpToF(LastCollisionDistNormalized * CameraData.MaxDistance, CameraData.MaxDistance, 1, Frametime) / CameraData.MaxDistance;
    //}
    if (bLastCollisionHit)
    {
        MaxDistanceInterp = LastCollisionDistNormalized * CameraData.MaxDistance;
    }
    //else
    {
        MaxDistanceInterp = InterpToF(MaxDistanceInterp, CameraData.MaxDistance, Config.SpringbackSpeed, Frametime);
        //MaxDistanceInterp = CameraData.MaxDistance;
        // printf("%f\n", MaxDistanceInterp);
    }

    {
        CameraOffset = GLMtoXMM(cameraOffsetInterp * lerp(MaxDistanceInterp / CameraData.MaxDistance, 1.f, 0.75f));
    }

    bLastCollisionHit = false;
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
eldenring.exe+3AF672 - 0FB6 08               - movzx ecx,byte ptr [rax]
eldenring.exe+3AF675 - 0F28 D7               - movaps xmm2,xmm7
eldenring.exe+3AF678 - 41 0F28 C8            - movaps xmm1,xmm8
eldenring.exe+3AF67C - 66 0F6E C1            - movd xmm0,ecx
eldenring.exe+3AF680 - 0F5B C0               - cvtdq2ps xmm0,xmm0
eldenring.exe+3AF683 - F3 0F59 05 E111E902   - mulss xmm0,[eldenring.exe+324086C]
eldenring.exe+3AF68B - E8 D043D8FF           - call eldenring.exe+133A60
*/
std::vector<uint16_t> PATTERN_CAMERA_SETTINGS = { 0x0F, 0xB6, 0x08, 0x0F, 0x28, 0xD7, 0x41, 0x0F, 0x28, 0xC8, 0x66, 0x0F, 0x6E, 0xC1, 0x0F, 0x5B, 0xC0, 0xF3, 0x0F, 0x59, 0x05, 0xE1, 0x11, 0xE9, 0x02, 0xE8, 0xD0, 0x43, 0xD8, 0xFF };
UHookRelativeIntermediate HookCameraSettings(
    "HookCameraSettings",
    PATTERN_CAMERA_SETTINGS,
    6,
    &GetCameraSettings
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
// offset camera collision traces' end point
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
eldenring.exe+3B8E88 - 76 0E                 - jna eldenring.exe+3B8E98
eldenring.exe+3B8E8A - F3 0F10 81 B4010000   - movss xmm0,[rcx+000001B4]
eldenring.exe+3B8E92 - F3 0F5E C1            - divss xmm0,xmm1
eldenring.exe+3B8E96 - EB 08                 - jmp eldenring.exe+3B8EA0
eldenring.exe+3B8E98 - F3 0F10 05 087AE802   - movss xmm0,[eldenring.exe+32408A8]
eldenring.exe+3B8EA0 - 0FC6 C0 00            - shufps xmm0,xmm0,00
*/
std::vector<uint16_t> PATTERN_MAX_DISTANCE_CLAMP = { 0x76, 0x0E, 0xF3, 0x0F, 0x10, 0x81, 0xB4, 0x01, 0x00, 0x00, 0xF3, 0x0F, 0x5E, 0xC1, 0xEB, 0x08, 0xF3, 0x0F, 0x10, 0x05, 0x08, 0x7A, 0xE8, 0x02, 0x0F, 0xC6, 0xC0, 0x00 };
UHookRelativeIntermediate HookMaxDistanceClamp(
    "HookMaxDistanceClamp",
    PATTERN_MAX_DISTANCE_CLAMP,
    8,
    &ClampMaxDistance,
    2
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
eldenring.exe+3B7DE9 - 0F28 99 F0020000      - movaps xmm3,[rcx+000002F0]
eldenring.exe+3B7DF0 - 0F28 A1 E0000000      - movaps xmm4,[rcx+000000E0]
eldenring.exe+3B7DF7 - 44 0F28 CB            - movaps xmm9,xmm3
eldenring.exe+3B7DFB - 44 0F5C CC            - subps xmm9,xmm4
eldenring.exe+3B7DFF - 66 44 0F7F 8D A0000000  - movdqa [rbp+000000A0],xmm9
eldenring.exe+3B7E08 - 66 44 0F7F 8D 90000000  - movdqa [rbp+00000090],xmm9
*/
std::vector<uint16_t> PATTERN_TARGET_OFFSET = { 0x0F, 0x28, 0x99, 0xF0, 0x02, 0x00, 0x00, 0x0F, 0x28, 0xA1, 0xE0, 0x00, 0x00, 0x00, 0x44, 0x0F, 0x28, 0xCB, 0x44, 0x0F, 0x5C, 0xCC, 0x66, 0x44, 0x0F, 0x7F, 0x8D, 0xA0, 0x00, 0x00, 0x00, 0x66, 0x44, 0x0F, 0x7F, 0x8D, 0x90, 0x00, 0x00, 0x00 };
// offset the lock-on target in world space to keep them centered on-screen
UHookRelativeIntermediate HookTargetOffset(
    "HookTargetOffset",
    PATTERN_TARGET_OFFSET,
    8,
    &SetTargetOffset,
    14
);

/*
eldenring.exe+3B8A10 - F3 44 0F10 AE E0020000  - movss xmm13,[rsi+000002E0]
eldenring.exe+3B8A19 - 41 0F28 E5            - movaps xmm4,xmm13
eldenring.exe+3B8A1D - F3 41 0F59 E5         - mulss xmm4,xmm13
eldenring.exe+3B8A22 - 0F2F E5               - comiss xmm4,xmm5
eldenring.exe+3B8A25 - 76 0D                 - jna eldenring.exe+3B8A34
eldenring.exe+3B8A27 - 44 0F29 8E 50010000   - movaps [rsi+00000150],xmm9
eldenring.exe+3B8A2F - E9 A8010000           - jmp eldenring.exe+3B8BDC
*/
std::vector<uint16_t> PATTERN_TARGET_VIEW_OFFSET = { 0xF3, 0x44, 0x0F, 0x10, 0xAE, 0xE0, 0x02, 0x00, 0x00, 0x41, 0x0F, 0x28, 0xE5, 0xF3, 0x41, 0x0F, 0x59, 0xE5, 0x0F, 0x2F, 0xE5, 0x76, 0x0D, 0x44, 0x0F, 0x29, 0x8E, 0x50, 0x01, 0x00, 0x00, 0xE9, 0xA8, 0x01, 0x00, 0x00 };
UHookRelativeIntermediate HookTargetViewOffset(
    "HookTargetViewOffset",
    PATTERN_TARGET_VIEW_OFFSET,
    9,
    &GetTargetViewOffset,
    0
);

/*
eldenring.exe+203DFA2 - 41 8B D4              - mov edx,r12d
eldenring.exe+203DFA5 - FF 50 28              - call qword ptr [rax+28]
eldenring.exe+203DFA8 - 4C 8D 44 24 40        - lea r8,[rsp+40]
eldenring.exe+203DFAD - 48 8B D6              - mov rdx,rsi
eldenring.exe+203DFB0 - 48 8B 4E 20           - mov rcx,[rsi+20]
eldenring.exe+203DFB4 - E8 F7120000           - call eldenring.exe+203F2B0
eldenring.exe+203DFB9 - 48 8B E8              - mov rbp,rax
eldenring.exe+203DFBC - 48 85 C0              - test rax,rax
eldenring.exe+203DFBF - 0F84 8A000000         - je eldenring.exe+203E04F
eldenring.exe+203DFC5 - 48 8B BE 80000000     - mov rdi,[rsi+00000080]
*/
std::vector<uint16_t> PATTERN_NPC_STATE = { 0x41, 0x8B, 0xD4, 0xFF, 0x50, 0x28, 0x4C, 0x8D, 0x44, 0x24, 0x40, 0x48, 0x8B, 0xD6, 0x48, 0x8B, 0x4E, 0x20, 0xE8, 0xF7, 0x12, 0x00, 0x00, 0x48, 0x8B, 0xE8, 0x48, 0x85, 0xC0, 0x0F, 0x84, 0x8A, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xBE, 0x80, 0x00, 0x00, 0x00 };
// this gets run during load-screens and sometimes return the opposite values
UHookRelativeIntermediate HookNPCState(
    "HookNPCState",
    PATTERN_NPC_STATE,
    7,
    &GetNPCState,
    35
);

/*
eldenring.exe+3B5AA9 - 8B 83 D4010000         - mov eax,[rbx+000001D4]
eldenring.exe+3B5AAF - 48 8D 4C 24 20         - lea rcx,[rsp+20]
eldenring.exe+3B5AB4 - 89 83 D0010000         - mov [rbx+000001D0],eax
eldenring.exe+3B5ABA - 89 93 60040000         - mov [rbx+00000460],edx
eldenring.exe+3B5AC0 - 48 C7 44 24 20 00000000 - mov qword ptr [rsp+20],00000000
eldenring.exe+3B5AC9 - 89 54 24 28            - mov [rsp+28],edx
eldenring.exe+3B5ACD - E8 5E0E9500            - call eldenring.exe+D06930
*/
std::vector<uint16_t> PATTERN_INTERACT_STATE = { 0x8B, 0x83, 0xD4, 0x01, 0x00, 0x00, 0x48, 0x8D, 0x4C, 0x24, 0x20, 0x89, 0x83, 0xD0, 0x01, 0x00, 0x00, 0x89, 0x93, 0x60, 0x04, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x89, 0x54, 0x24, 0x28, 0xE8, 0x5E, 0x0E, 0x95, 0x00 };
// this may not work on future game versions
UHookRelativeIntermediate HookInteractState(
    "HookInteractState",
    PATTERN_INTERACT_STATE,
    6,
    &GetInteractState
);

/*
; runs every frame during riposte animations except for certain frames
; doesn't run when the animations end tho so we can't just set a bool
eldenring.exe+560B1F - 74 1F                 - je eldenring.exe+560B40
eldenring.exe+560B21 - 0FB6 00               - movzx eax,byte ptr [rax]
eldenring.exe+560B24 - 83 E0 01              - and eax,01
eldenring.exe+560B27 - 84 C0                 - test al,al
eldenring.exe+560B29 - 74 15                 - je eldenring.exe+560B40
eldenring.exe+560B2B - E8 C040CFFF           - call eldenring.exe+254BF0
eldenring.exe+560B30 - 48 85 C0              - test rax,rax
eldenring.exe+560B33 - 74 0B                 - je eldenring.exe+560B40      ; pattern starts
eldenring.exe+560B35 - 0FB6 DB               - movzx ebx,bl
eldenring.exe+560B38 - 33 C9                 - xor ecx,ecx
eldenring.exe+560B3A - 38 48 11              - cmp [rax+11],cl
eldenring.exe+560B3D - 0F44 D9               - cmove ebx,ecx
eldenring.exe+560B40 - 80 7E 28 00           - cmp byte ptr [rsi+28],00
*/
std::vector<uint16_t> PATTERN_CRITICAL = { 0x74, 0x0B, 0x0F, 0xB6, 0xDB, 0x33, 0xC9, 0x38, 0x48, 0x11, 0x0F, 0x44, 0xD9, 0x80, 0x7E, 0x28, 0x00 };
UHookRelativeIntermediate HookCriticalAtk(
    "HookCriticalAtk",
    PATTERN_CRITICAL,
    5,
    &SetCrit,
    2
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
    //std::string configPath = ModUtils::GetModuleFolderPath() + "\\config.ini";
    //ModUtils::Log("config file path: %s", configPath.c_str());
    //if (!UConfig::CheckConfigFile(configPath))
    //{
    //    ModUtils::Log("Creating config file");
    //    std::ofstream file;
    //    file.open(configPath);
    //    file << UConfig::GetDefaultConfig();
    //    file.close();
    //}
    //Config = FModConfig(INIReader(configPath));
    Config.ReadFile(ModUtils::GetModuleFolderPath() + "\\config.ini");

    std::vector<UHookRelativeIntermediate*> hooks {
        &HookGetFrametime,
        &HookGetCameraData,
        &HookCameraSettings,
        &HookSetCameraOffset,
        &HookSetCollisionOffsetAlt,
        &HookSetCollisionOffsetAlt1,
        //&HookCollisionAdjust,
        //&HookCollisionAdjust01,
        &HookCollisionAdjust1,
        &HookTargetOffset,
        //&HookNPCState,
    };
    if (Config.bUseAutoDisable)
    {
        hooks.push_back(&HookInteractState);
        hooks.push_back(&HookCriticalAtk);
    }
    if (Config.SpringbackSpeed > 0)
    {
        hooks.push_back(&HookMaxDistanceClamp);
    }
    if (Config.bUseTargetOffset || Config.bUseSideSwitch || Config.TargetViewOffsetMul != 1.f)
    {
        hooks.push_back(&HookTargetViewOffset);
    }
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
        //uintptr_t addr = ModUtils::SigScan({ 0x0F, 0x84, 0xC3, 0x00, 0x00, 0x00, 0x0F, 0x28, 0x65, 0xD0, 0x0F, 0x28, 0xD4, 0x0F, 0x59, 0xD4, 0x0F, 0x28, 0xCA, 0x0F, 0xC6, 0xCA, 0x66, 0xF3, 0x0F, 0x58, 0xD1 });
        //if (addr)
        //{
        //    *reinterpret_cast<char*>(addr) = 0x90;
        //    *reinterpret_cast<char*>(addr+1) = 0xE9;
        //}
    }

#ifdef _DEBUG
    while (true)
    {
        if (ModUtils::CheckHotkey(0x70))
        {
            printf("CamBaseAddr = %p\n", CamBaseAddr);
            printf("ParamID = %d\n", CameraData.ParamID);
            printf("%p\n", InteractPtr);
            //printf("%s\n", glm::to_string(CameraData.DirForward).c_str());
            //printf("LocFinal = %s\n", glm::to_string(CameraData.LocFinal).c_str());
            //printf("LocPivot = %s\n", glm::to_string(CameraData.LocPivot).c_str());
            //printf("LocPivotInterp = %s\n", glm::to_string(CameraData.LocPivotInterp).c_str());
            //printf("LocTarget = %s\n", glm::to_string(CameraData.LocTarget).c_str());
            //printf("Rotation = %s\n", glm::to_string(CameraData.Rotation).c_str());
            //printf("%f %f\n", CameraData.MaxDistance, saturate(glm::distance(CameraData.LocPivotInterp, CameraData.LocFinalInterp) / CameraData.MaxDistance));
        }
        if (ModUtils::CheckHotkey(0x71))
        {
            for (UHookRelativeIntermediate* pHook : hooks)
            {
                pHook->Toggle();
            }
        }
        if (ModUtils::CheckHotkey(0x72))
        {
            SideSwitch = -SideSwitch;
        }
        //if (ModUtils::CheckHotkey(0x73))
        //{
        //    HookSetCollisionOffsetAlt1.Toggle();
        //}
        Sleep(2);
    }
#endif

    ModUtils::CloseLog();
    return 0;
}

#pragma warning(suppress: 4100)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        UConfig::ResourceModule = hModule;

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

