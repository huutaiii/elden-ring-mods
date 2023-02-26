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

    float OffsetInterpSpeed = 0;
    float SpringbackSpeed = 0;
    float TargetViewOffsetMul = 1.f;
    float TargetAimAreaMul = 1.f;
    float LookAhead = 0.f;
    float LookAheadZ = -1.f;

    bool bUseSideSwitch = false;
    bool bUseTargetOffset = false;
    bool bUseAutoDisable = false;

    struct UKeys
    {
        int Toggle = 0;
        int ToggleOffset = 0;
    } Keys;

    void ReadINI(INIReader ini)
    {
        Offset = ini.GetVec("main", "offset", Offset);
        OffsetLockon = ini.GetVec("main", "offset-lockon", Offset);

        OffsetInterpSpeed = ini.GetReal("main", "interpolation-speed", OffsetInterpSpeed);
        SpringbackSpeed = ini.GetReal("main", "collision-springback-speed", SpringbackSpeed);
        TargetViewOffsetMul = ini.GetReal("main", "target-view-offset-multiplier", TargetViewOffsetMul);
        TargetAimAreaMul = ini.GetReal("main", "target-aim-area-multiplier", TargetAimAreaMul);
        LookAhead = ini.GetReal("main", "look-ahead", LookAhead);
        LookAheadZ = ini.GetReal("main", "look-ahead-z", LookAheadZ);

        bUseSideSwitch = ini.GetBoolean("main", "dynamic-lockon-offset", bUseSideSwitch);
        bUseTargetOffset = ini.GetBoolean("main", "use-offset-on-target", bUseTargetOffset);
        bUseAutoDisable = ini.GetBoolean("main", "auto-toggle", false);

        Keys.Toggle = ini.GetInteger("keybinding", "toggle-hooks", Keys.Toggle);
        Keys.ToggleOffset = ini.GetInteger("keybinding", "toggle-offset", Keys.ToggleOffset);

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

private:
    glm::vec4 PrevLocPivot;

public:
    glm::vec3 PivotVelocity;

    void Update(LPVOID BaseAddr = nullptr)
    {
        if (!BaseAddr)
        {
            return;
        }

        BaseAddress = BaseAddr;
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

        PivotVelocity = LocPivot - PrevLocPivot;
        PrevLocPivot = LocPivot;
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
    void SetCollisionOffsetAlt();
    void SetCollisionOffset();
    void AdjustCollision();
    void ClampMaxDistance();
    void SetTargetOffset();
    void TargetViewYOffset();

    void GetTargetViewOffset();
    __m128 TargetViewOffset;
    float TargetViewMaxOffset;
    float TargetViewOffsetMul;

    float TargetAimAreaMul;

    void GetInteractState();
    void SetCrit();

    WORD CritAnimElapsed;
}

// Called by asm hook to make sure the pointer is always valid
extern "C" void ReadCameraData()
{
    CameraData.Update(CamBaseAddr);
}

float LockonAlpha = 0.f;
float SideSwitch = 1.f;
float ToggleAlpha = 1.f;
const int EnableOffsetDelay = 2;
int EnableOffsetElapsed = 0;
const WORD CritAnimDelay = 15;

bool Control = true;

extern "C" void CalcCameraOffset()
{
    if (!Control)
    {
        TargetOffset = _mm_set_ps(0, 0, 0, 0);
        MaxDistanceInterp = CameraData.MaxDistance;
        TargetAimAreaMul = 1.f;
        TargetViewOffsetMul = 1.f;
        CollisionOffset = _mm_set_ps(0, 0, 0, 0);
        RetractCollisionOffset = _mm_set_ps(0, 0, 0, 0);
        CameraOffset = GLMtoXMM(InterpToV(XMMtoGLM(CameraOffset), glm::vec4(0), 5.f, Frametime));
        return;
    }

    TargetViewOffsetMul = Config.TargetViewOffsetMul;
    TargetAimAreaMul = Config.TargetAimAreaMul;

    LockonAlpha = InterpToF(LockonAlpha, CameraData.bIsLockedOn ? 1.f : 0.f, CameraData.bIsLockedOn ? 3.f : 2.f, Frametime);

    if (Config.bUseSideSwitch)
    {
        if (CameraData.bIsLockedOn)
        {

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

    float offsetAlpha = ToggleAlpha * (1.f - CritAlpha);
    glm::vec3 localMaxOffset = lerp(Config.Offset, Config.OffsetLockon, LockonAlpha) * offsetAlpha;

    {
        static float GraceInterp = 0.f;
        GraceInterp = InterpToF(GraceInterp, CameraData.ParamID == ID_GRACE ? 1.f : 0.f, 3.f, Frametime);
        localMaxOffset += glm::vec3(-0.5f, 0, 0) * GraceInterp;
    }

    glm::mat4 rotation = glm::rotateNormalizedAxis(glm::mat4(1), CameraData.Rotation.y, glm::vec3(0, 1, 0));

    if (Config.LookAhead > 0.f)
    {
        static glm::vec3 MovementOffsetInterp = {};

        glm::vec3 localvelocity = glm::inverse(rotation) * glm::vec4(CameraData.PivotVelocity, 0);
        localvelocity = ClampVecLength(localvelocity, 1.f);
        glm::vec3 scale = glm::vec3(Config.LookAhead * 10.f);
        scale.y = 0.f;
        if (Config.LookAheadZ >= 0.f)
        {
            scale.z = Config.LookAheadZ;
        }

        bool bIsMoving = glm::length(localvelocity) > 0.0001f;
        float speed = max(scale.x, scale.z);
        MovementOffsetInterp = InterpToV(MovementOffsetInterp, CameraData.bIsLockedOn ? glm::vec3(0) : localvelocity * scale, bIsMoving ? speed / 6 : speed / 8, Frametime);

        localMaxOffset += MovementOffsetInterp * offsetAlpha;
    }
    

    float RetractAlpha = saturate(glm::distance(CameraData.LocPivotInterp, CameraData.LocFinalInterp) / CameraData.MaxDistance);
    glm::vec3 cameraOffset = rotation * glm::vec4(localMaxOffset * glm::vec3(lerp(1.f, SideSwitch, LockonAlpha), 1, 1), 1);
    glm::vec3 collisionOffset = cameraOffset;

    static float TargetDistAdjust = 1.f;
    if (CameraData.bIsLockedOn)
    {
        // distance in Y plane
        float targetDistance = glm::length((CameraData.LocTarget - CameraData.LocPivotInterp).xyz * glm::vec3(1, 0, 1));
        //glm::vec3 targetOffset = cameraOffset * (0.0625f * (-1.f) * pow(targetDistance > 1.f ? targetDistance : pow(targetDistance, 1 / targetDistance), 1.125f));

        // clamp offset when locked on to avoid the camera spinning around
        TargetDistAdjust = 1.f - 1 / exp2(targetDistance);
        TargetDistAdjust *= TargetDistAdjust;

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
0FB6080F28D7410F28C8660F6EC10F5BC0(F30F5905????????E8????????)
*/
std::vector<uint16_t> PATTERN_CAMERA_SETTINGS = { 0x0F, 0xB6, 0x08, 0x0F, 0x28, 0xD7, 0x41, 0x0F, 0x28, 0xC8, 0x66, 0x0F, 0x6E, 0xC1, 0x0F, 0x5B, 0xC0 };
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
eldenring.exe+3B8EF6 - 4C 8D 44 24 60        - lea r8,[rsp+60]
eldenring.exe+3B8EFB - 0F29 44 24 50         - movaps [rsp+50],xmm0
eldenring.exe+3B8F00 - 48 8B C8              - mov rcx,rax
eldenring.exe+3B8F03 - F3 0F10 83 C0010000   - movss xmm0,[rbx+000001C0]
eldenring.exe+3B8F0B - F3 0F11 44 24 20      - movss [rsp+20],xmm0
eldenring.exe+3B8F11 - E8 FA688800           - call eldenring.exe+C3F810
eldenring.exe+3B8F16 - 84 C0                 - test al,al
eldenring.exe+3B8F18 - 74 5C                 - je eldenring.exe+3B8F76
4C 8D 44 24 60 0F29 44 24 50 48 8B C8 F3 0F10 83 ????0000
*/
std::vector<uint16_t> PATTERN_COLLISION_DISABLE = { 0x4C, 0x8D, 0x44, 0x24, 0x60, 0x0F, 0x29, 0x44, 0x24, 0x50, 0x48, 0x8B, 0xC8, 0xF3, 0x0F, 0x10, 0x83, MASK, MASK, 0x00, 0x00 };
// Disable the collision check that make the camera turn around
UMemReplace DisableCollisionCheck(
    "DisableCollisionCheck",
    PATTERN_COLLISION_DISABLE,
    std::vector<uint8_t>{ 0x48, 0x31, 0xC0 }, // xor rax, rax
    5,
    27
);

/*
eldenring.exe+3B583D - BA 5B000000           - mov edx,0000005B
eldenring.exe+3B5842 - 48 8B CF              - mov rcx,rdi
eldenring.exe+3B5845 - 0F29 75 D0            - movaps [rbp-30],xmm6
eldenring.exe+3B5849 - F3 0F11 44 24 20      - movss [rsp+20],xmm0
eldenring.exe+3B584F - E8 BC9F8800           - call eldenring.exe+C3F810
eldenring.exe+3B5854 - 84 C0                 - test al,al
eldenring.exe+3B5856 - 0F84 C3000000         - je eldenring.exe+3B591F
BA ????0000 48 8B CF 0F29 75 D0 F3 0F11 44 24 20 E8 ????????
*/
std::vector<uint16_t> PATTERN_COLLISION_OFFSET = { 0xBA, MASK, MASK, 0x00, 0x00, 0x48, 0x8B, 0xCF, 0x0F, 0x29, 0x75, 0xD0, 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x20, 0xE8, MASK, MASK, MASK, MASK };
// offset camera collision traces' end point
UHookRelativeIntermediate HookSetCollisionOffset(
    "HookCollisionOffset",
    PATTERN_COLLISION_OFFSET,
    5,
    &SetCollisionOffset
);

/*
eldenring.exe+3B5911 - 0F59 D4               - mulps xmm2,xmm4
eldenring.exe+3B5914 - 0F59 D0               - mulps xmm2,xmm0
eldenring.exe+3B5917 - 0F58 55 E0            - addps xmm2,[rbp-20]
eldenring.exe+3B591B - 0F29 53 40            - movaps [rbx+40],xmm2
eldenring.exe+3B591F - 48 8B 4D 00           - mov rcx,[rbp+00]
eldenring.exe+3B5923 - 48 33 CC              - xor rcx,rsp
eldenring.exe+3B5926 - E8 B5BA0F02           - call eldenring.exe+24B13E0
0F59 D4 0F59 D0 0F58 55 E0 0F29 53 40 48 8B 4D 00
*/
std::vector<uint16_t> PATTERN_COLLISION_ADJUST1 = { 0x0F, 0x59, 0xD4, 0x0F, 0x59, 0xD0, 0x0F, 0x58, 0x55, 0xE0, 0x0F, 0x29, 0x53, 0x40, 0x48, 0x8B, 0x4D, 0x00 };
// the collision function sets a location for the camera when the collision trace hits
// which is unintentionally offset
// this reverts the offset
UHookRelativeIntermediate HookCollisionAdjust(
    "HookCollisionAdjust",
    PATTERN_COLLISION_ADJUST1,
    6,
    &AdjustCollision
);

/*
eldenring.exe+3B8E88 - 76 0E                 - jna eldenring.exe+3B8E98
eldenring.exe+3B8E8A - F3 0F10 81 B4010000   - movss xmm0,[rcx+000001B4]
eldenring.exe+3B8E92 - F3 0F5E C1            - divss xmm0,xmm1
eldenring.exe+3B8E96 - EB 08                 - jmp eldenring.exe+3B8EA0
eldenring.exe+3B8E98 - F3 0F10 05 087AE802   - movss xmm0,[eldenring.exe+32408A8]
eldenring.exe+3B8EA0 - 0FC6 C0 00            - shufps xmm0,xmm0,00
76 0E F3 0F10 81 ????0000 F3 0F5E C1 EB 08 F3 0F10 05 ????????
*/
std::vector<uint16_t> PATTERN_MAX_DISTANCE_CLAMP = { 0x76, 0x0E, 0xF3, 0x0F, 0x10, 0x81, MASK, MASK, 0x00, 0x00, 0xF3, 0x0F, 0x5E, 0xC1, 0xEB, 0x08, 0xF3, 0x0F, 0x10, 0x05, MASK, MASK, MASK, MASK };
UHookRelativeIntermediate HookMaxDistanceClamp(
    "HookMaxDistanceClamp",
    PATTERN_MAX_DISTANCE_CLAMP,
    8,
    &ClampMaxDistance,
    2
);

/*
eldenring.exe+3B7DE9 - 0F28 99 F0020000      - movaps xmm3,[rcx+000002F0]
eldenring.exe+3B7DF0 - 0F28 A1 E0000000      - movaps xmm4,[rcx+000000E0]
eldenring.exe+3B7DF7 - 44 0F28 CB            - movaps xmm9,xmm3
eldenring.exe+3B7DFB - 44 0F5C CC            - subps xmm9,xmm4
eldenring.exe+3B7DFF - 66 44 0F7F 8D A0000000  - movdqa [rbp+000000A0],xmm9
eldenring.exe+3B7E08 - 66 44 0F7F 8D 90000000  - movdqa [rbp+00000090],xmm9
0F28 99 ????0000 0F28 A1 ????0000 44 0F28 CB 44 0F5C CC
*/
std::vector<uint16_t> PATTERN_TARGET_OFFSET = { 0x0F, 0x28, 0x99, MASK, MASK, 0x00, 0x00, 0x0F, 0x28, 0xA1, MASK, MASK, 0x00, 0x00, 0x44, 0x0F, 0x28, 0xCB, 0x44, 0x0F, 0x5C, 0xCC };
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
F3 44 0F10 AE ????0000 41 0F28 E5 F3 41 0F59 E5 0F2F E5
*/
std::vector<uint16_t> PATTERN_TARGET_VIEW_OFFSET = { 0xF3, 0x44, 0x0F, 0x10, 0xAE, MASK, MASK, 0x00, 0x00, 0x41, 0x0F, 0x28, 0xE5, 0xF3, 0x41, 0x0F, 0x59, 0xE5, 0x0F, 0x2F, 0xE5 };
UHookRelativeIntermediate HookTargetViewOffset(
    "HookTargetViewOffset",
    PATTERN_TARGET_VIEW_OFFSET,
    9,
    &GetTargetViewOffset,
    0
);

/*
eldenring.exe+3B5E5E - F3 0F10 83 E4020000   - movss xmm0,[rbx+000002E4]
eldenring.exe+3B5E66 - F3 44 0F5C E8         - subss xmm13,xmm0
eldenring.exe+3B5E6B - F3 44 0F11 63 50      - movss [rbx+50],xmm12
eldenring.exe+3B5E71 - F3 44 0F11 9B 58020000  - movss [rbx+00000258],xmm11
eldenring.exe+3B5E7A - F3 44 0F59 EE         - mulss xmm13,xmm6
eldenring.exe+3B5E7F - F3 44 0F58 E8         - addss xmm13,xmm0
eldenring.exe+3B5E84 - F3 0F10 83 94010000   - movss xmm0,[rbx+00000194]

F3 0F10 83 ????0000 F3 44 0F5C E8 F3 44 0F11 63 ?? F3 44 0F11 9B ????0000

F3 44 0F5C F0 F3 44 0F11 AB ????0000 44 0F28 6C 24 ?? F3 44 0F59 F6 //F3 44 0F58 F0
*/
std::vector<uint16_t> PATTERN_TARGET_Y_OFFSET = { 0xF3, 0x0F, 0x10, 0x83, MASK, MASK, 0x00, 0x00, 0xF3, 0x44, 0x0F, 0x5C, 0xE8, 0xF3, 0x44, 0x0F, 0x11, 0x63, MASK, 0xF3, 0x44, 0x0F, 0x11, 0x9B, MASK, MASK, 0x00, 0x00 };
UHookRelativeIntermediate HookTargetYOffset(
    "HookTargetYOffset",
    PATTERN_TARGET_Y_OFFSET,
    8,
    &TargetViewYOffset
);

/*
eldenring.exe+3B5AA9 - 8B 83 D4010000         - mov eax,[rbx+000001D4]
eldenring.exe+3B5AAF - 48 8D 4C 24 20         - lea rcx,[rsp+20]
eldenring.exe+3B5AB4 - 89 83 D0010000         - mov [rbx+000001D0],eax
eldenring.exe+3B5ABA - 89 93 60040000         - mov [rbx+00000460],edx
eldenring.exe+3B5AC0 - 48 C7 44 24 20 00000000 - mov qword ptr [rsp+20],00000000
eldenring.exe+3B5AC9 - 89 54 24 28            - mov [rsp+28],edx
eldenring.exe+3B5ACD - E8 5E0E9500            - call eldenring.exe+D06930
8B 83 ????0000 48 8D 4C 24 ?? 89 83 ????0000 89 93 ????0000 48 C7 44 24 ?? 00000000
*/
std::vector<uint16_t> PATTERN_INTERACT_STATE = { 0x8B, 0x83, MASK, MASK, 0x00, 0x00, 0x48, 0x8D, 0x4C, 0x24, MASK, 0x89, 0x83, MASK, MASK, 0x00, 0x00, 0x89, 0x93, MASK, MASK, 0x00, 0x00, 0x48, 0xC7, 0x44, 0x24, MASK, 0x00, 0x00, 0x00, 0x00 };
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
74 0B 0FB6 DB 33 C9 38 48 11 0F44 D9 80 7E ?? 00
                          --
*/
std::vector<uint16_t> PATTERN_CRITICAL = { 0x74, 0x0B, 0x0F, 0xB6, 0xDB, 0x33, 0xC9, 0x38, 0x48, 0x11, 0x0F, 0x44, 0xD9, 0x80, 0x7E, MASK, 0x00 };
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
    Config.ReadFile(ModUtils::GetModuleFolderPath() + "\\config.ini");

    std::vector<UModSwitch*> hooks {
        &HookGetFrametime,
        &HookGetCameraData,
        &HookCameraSettings,
        &HookSetCameraOffset,
        &DisableCollisionCheck,
        &HookSetCollisionOffset,
        &HookCollisionAdjust,
        &HookTargetOffset,
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
    if (Config.bUseTargetOffset || Config.bUseSideSwitch || Config.TargetAimAreaMul != 1.f)
    {
        hooks.push_back(&HookTargetViewOffset);
    }
    if (Config.TargetViewOffsetMul != 1.f)
    {
        hooks.push_back(&HookTargetYOffset);
    }
    for (UModSwitch* hook : hooks)
    {
        hook->Enable();
    }


#ifdef _DEBUG
    while (true)
    {
        if (ModUtils::CheckHotkey(0x70))
        {
            printf("CamBaseAddr = %p\n", CamBaseAddr);
        }
        if (ModUtils::CheckHotkey(0x71))
        {
            for (UModSwitch* pHook : hooks)
            {
                pHook->Toggle();
            }
        }
        Sleep(2);
    }
#else
    if (Config.Keys.Toggle || Config.Keys.ToggleOffset)
    {
        while (true)
        {
            if (Config.Keys.Toggle && ModUtils::CheckHotkey(Config.Keys.Toggle))
            {
                for (UModSwitch* pHook : hooks)
                {
                    pHook->Toggle();
                }
            }
            if (Config.Keys.ToggleOffset && ModUtils::CheckHotkey(Config.Keys.ToggleOffset))
            {
                Control = !Control;
            }
        }
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

