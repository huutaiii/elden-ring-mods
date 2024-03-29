#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>
#include <functional>

#include "ModUtils.h"

// See VirtualMAlloc::Get, VirtualMAlloc::Alloc
class MVirtualAlloc {
    DWORD processId = GetCurrentProcessId();
    DWORD_PTR baseAddress = ModUtils::GetProcessBaseAddress(GetCurrentProcessId());

private:
    SYSTEM_INFO sys;
    MVirtualAlloc()
    {
        GetSystemInfo(&sys);
        ModUtils::Log("Process base address: %p", baseAddress);
        ModUtils::Log("System page size: %u", sys.dwPageSize);
        ModUtils::Log("System allocation granularity: %u", sys.dwAllocationGranularity);
    };

public:
    // Get instance
    static MVirtualAlloc& Get()
    {
        static MVirtualAlloc instance;
        return instance;
    }

private:
    LPBYTE lpScan = 0;
    MEMORY_BASIC_INFORMATION memInfo;

    LPVOID lpCurrent = 0;
    DWORD bytesAllocated = 0;
    DWORD currentPageSize = 0;

    LPVOID Scan()
    {
        lpScan = lpScan ? lpScan : (LPBYTE)baseAddress;
        SIZE_T numBytes;

        do
        {
            lpScan -= sys.dwAllocationGranularity;
            numBytes = VirtualQuery(lpScan, &memInfo, sizeof(memInfo));

            lpScan = static_cast<LPBYTE>(memInfo.BaseAddress);

            if (memInfo.State == MEM_FREE)
            {
                return lpScan;
            }
        }
        while (numBytes);

        return nullptr;
    }

public:

    // Allocate memory below process base address
    // This allows for memory hooks using shorter jump ops (eg. 0xE9)
    // Subsequent calls usually access the same memory page
    // see VirtualAlloc function
    // 
    // dwSize in [1..4096]
    LPVOID Alloc(SIZE_T dwSize, SIZE_T alignment = 1, DWORD flAllocType = MEM_RESERVE | MEM_COMMIT, DWORD flProtec = PAGE_EXECUTE_READWRITE)
    {
        size_t rem = bytesAllocated % alignment;
        size_t padding = rem ? 0 : alignment - (rem);

        bytesAllocated += padding + dwSize;

        if (!lpCurrent || bytesAllocated > currentPageSize)
        {
            LPVOID lpAlloc = Scan();

            if (!lpAlloc)
            {
                ModUtils::RaiseError("Cannot allocate memory");
                return nullptr;
            }

            ModUtils::Log("Allocating page at: %p", lpScan);

            // Preallocate a region equals to system page size (typically 4KiB)
            lpCurrent = VirtualAlloc(lpAlloc, sys.dwPageSize, flAllocType, flProtec);

            bytesAllocated = static_cast<DWORD>(dwSize);
            currentPageSize = static_cast<DWORD>(sys.dwPageSize);
        }

        return (LPBYTE)lpCurrent + bytesAllocated - dwSize;
    }

    // there's no deallocation 'cause we don't need it, for now
};

class UModSwitch
{
protected:
    bool bIsEnabled = false;
public:
    virtual void Enable() { bIsEnabled = true; }
    virtual void Disable() { bIsEnabled = false; }
    virtual void Toggle() { bIsEnabled ? Disable() : Enable(); }
    virtual bool IsEnabled() { return bIsEnabled; }

    virtual std::string GetName() = 0;
};

// writes an absolute jump to destination at specified address (14 bytes)
class UHookAbsoluteNoCopy : public UModSwitch
{
    static const uint16_t op = 0x25ff;

    LPVOID lpHook;
    LPVOID lpDest;

public:
    void Enable()
    {
        if (lpHook && lpDest)
        {
            *static_cast<uint64_t*>(lpHook) = static_cast<uint64_t>(op);
            *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(lpHook) + 6) = reinterpret_cast<uint64_t>(lpDest);
        }
    }

    UHookAbsoluteNoCopy(LPVOID lpHook = nullptr, LPVOID lpDestination = nullptr, size_t offset = 0) : lpDest(lpDestination)
    {
        this->lpHook = static_cast<LPBYTE>(lpHook) + offset;
    }

    std::string GetName() { return ""; }
};

// Creates or removes a hook using a relative jump (5 bytes)
// First jump to an intermediate address at which we then do an absolute jump to the custom code
// This way we don't have to determine the size of the custom asm
// We also copy the stolen bytes over to the intermediate location so the custom code can omit the original code
class UHookRelativeIntermediate : public UModSwitch
{
public:
    static const uint8_t OpCall = 0xE8;
    static const uint8_t OpJmp = 0xE9;
    static const unsigned char RelJmpSize = 5;

    const std::string msg;
private:
    MVirtualAlloc& allocator = MVirtualAlloc::Get();

    LPVOID lpHook = nullptr;
    LPVOID lpIntermediate = nullptr;
    bool bExecuteOriginal = true;

    LPVOID lpDestination;
    size_t numBytes;

    bool bUseCall = true;

    bool bCanHook = false;
    bool bEnabled = false;
    std::unique_ptr<UHookAbsoluteNoCopy> upJmpAbs;
    size_t JumpOffset;
    size_t StolenBytesOffset;

    std::function<void()> fnEnable = []() {};
    std::function<void()> fnDisable = []() {};

    static const std::vector<uint8_t> RSPUp; // lea rsp,[rsp+8] (5B)
    static const std::vector<uint8_t> RSPDown; // lea rsp,[rsp-8] (5B)

    // Initialize the intermediate code that we can decide to jump to later
    void Init()
    {
        size_t SPDownOffset = RSPUp.size() + numBytes;

        // we still need to store the stolen code somewhere even when we don't want to execute it
        if (bUseCall)
        {
            lpIntermediate = allocator.Alloc(numBytes + 14 + RSPUp.size() + RSPDown.size()); // one 14B jump, two 3B adds
            JumpOffset = numBytes + RSPUp.size() + RSPDown.size();
            StolenBytesOffset = RSPUp.size();
        }
        else
        {
            lpIntermediate = allocator.Alloc(numBytes + 14);
            JumpOffset = numBytes;
            StolenBytesOffset = 0;
        }

        if (!lpIntermediate)
        {
            ModUtils::Log("Cannot allocate memory for hook: %s", msg.c_str());
            return;
        }

        if (bUseCall)
        {
            // move stack pointer up so stolen instructions can access the stack
            ModUtils::MemCopy(uintptr_t(lpIntermediate), uintptr_t(RSPUp.data()), RSPUp.size());
        }

        // copy to be stolen bytes to the imtermediate location
        ModUtils::MemCopy(reinterpret_cast<uint64_t>(lpIntermediate) + StolenBytesOffset, reinterpret_cast<uint64_t>(lpHook), numBytes);

        if (bUseCall)
        {
            // write an instruction to move the stack pointer back down
            ModUtils::MemCopy(uintptr_t(lpIntermediate) + SPDownOffset, uintptr_t(RSPDown.data()), RSPDown.size());
        }

        // create jump from intermediate code to custom code
        upJmpAbs = std::make_unique<UHookAbsoluteNoCopy>(lpIntermediate, lpDestination, JumpOffset);
        upJmpAbs->Enable();

        ModUtils::Log("Generated hook '%s' from %p to %p at %p", msg.c_str(), lpHook, lpDestination, lpIntermediate);
    }

private:

    inline void UHookRelativeIntermediate_Internal(std::vector<uint16_t> pattern, int offset, uintptr_t *pReturnAddress)
    {
        lpHook = reinterpret_cast<LPVOID>(ModUtils::SigScan(pattern, msg, true) + offset);
        bCanHook = lpHook != nullptr;
        bUseCall = pReturnAddress == nullptr;
        if (!bUseCall)
        {
            *pReturnAddress = reinterpret_cast<uintptr_t>((char*)lpHook + numBytes);
        }
        ModUtils::Log("Hook target (%s): %p (rel: %#x)", msg.c_str(), lpHook, UINT_PTR(lpHook) - ModUtils::GetProcessBaseAddress(GetCurrentProcessId()));
    }

public:

    UHookRelativeIntermediate(UHookRelativeIntermediate&) = delete;

    UHookRelativeIntermediate(
        std::vector<uint16_t> signature,
        size_t numStolenBytes,
        LPVOID destination,
        int offset = 0,
        bool bExecuteOriginal = true,
        uintptr_t *pReturnAddress = nullptr,
        std::string msg = "Unknown Hook",
        std::function<void()> enable = []() {},
        std::function<void()> disable = []() {}
    )
        : numBytes(numStolenBytes), lpDestination(destination), msg(msg), fnEnable(enable), fnDisable(disable), bExecuteOriginal(bExecuteOriginal)
    {
        UHookRelativeIntermediate_Internal(signature, offset, pReturnAddress);
    }

    UHookRelativeIntermediate(
        std::string id,
        std::vector<uint16_t> signature,
        size_t numStolenBytes,
        LPVOID destination,
        int offset = 0
    )
        : numBytes(numStolenBytes), lpDestination(destination), msg(id)
    {
        UHookRelativeIntermediate_Internal(signature, offset, nullptr);
    }

    UHookRelativeIntermediate(
        std::string id,
        std::vector<uint16_t> signature,
        size_t numStolenBytes,
        LPVOID destination,
        bool bExecuteOriginal,
        int offset = 0
    )
        : numBytes(numStolenBytes), lpDestination(destination), msg(id), bExecuteOriginal(bExecuteOriginal)
    {
        UHookRelativeIntermediate_Internal(signature, offset, nullptr);
    }

    UHookRelativeIntermediate(
        std::string id,
        std::vector<uint16_t> signature,
        size_t numStolenBytes,
        LPVOID destination,
        uintptr_t* pReturnAddress,
        int offset = 0
    )
        : msg(id), numBytes(numStolenBytes), lpDestination(destination)
    {
        UHookRelativeIntermediate_Internal(signature, offset, pReturnAddress);
    }

    const bool HasFoundSignature() const { return bCanHook; }

    std::string GetName() override { return msg; }

    void Enable() override
    {
        if (bEnabled) { return; }

        if (!lpIntermediate)
        {
            Init();
        }

        if (!bCanHook || !lpHook || !lpIntermediate)
        {
            ModUtils::RaiseError("Hook activation failed: " + msg);
            return;
        }

        ModUtils::Log("Enabling hook '%s' from %p to %p", msg.c_str(), lpHook, lpDestination);

        // pad the jump in case numBytes > jump instruction size
        ModUtils::MemSet(reinterpret_cast<uintptr_t>(lpHook), 0x90, numBytes);

        // write instruction at hook address
        *static_cast<uint8_t*>(lpHook) = bUseCall ? OpCall : OpJmp;
        uint32_t relOffset = static_cast<uint32_t>(static_cast<uint8_t*>(lpIntermediate) - static_cast<uint8_t*>(lpHook) - RelJmpSize);
        relOffset += uint32_t(bExecuteOriginal ? 0 : JumpOffset);

        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(lpHook) + 1) = relOffset;

        fnEnable();
        bEnabled = true;
    }
    void Disable() override
    {
        if (!bEnabled) { return; }

        ModUtils::Log("Disabling hook '%s' from %p to %p", msg.c_str(), lpHook, lpDestination);
        ModUtils::MemCopy(uintptr_t(lpHook), uintptr_t(lpIntermediate) + StolenBytesOffset, numBytes);

        fnDisable();
        bEnabled = false;
    }
    void Toggle() override
    {
        bEnabled ? Disable() : Enable();
    }
    ~UHookRelativeIntermediate() { Disable(); }
};

const std::vector<uint8_t> UHookRelativeIntermediate::RSPUp({ 0x48, 0x8D, 0x64, 0x24, 0x08 });
const std::vector<uint8_t> UHookRelativeIntermediate::RSPDown({ 0x48, 0x8D, 0x64, 0x24, 0xf8 });

class UMemReplace : public UModSwitch
{
    unsigned int Length;
    LPVOID lpDestination = nullptr;
    LPVOID lpBackup = nullptr;
    std::string Msg;
    std::vector<unsigned char> Replacement;

public:
    UMemReplace(std::string name, std::vector<uint16_t> signature, std::vector<uint8_t> replacement, unsigned int length, int offset = 0)
        : Msg(name), Length(length), Replacement(replacement)
    {
        if (replacement.size() > length)
        {
            ModUtils::RaiseError(Msg + ": Invalid arguments");
        }

        lpDestination = reinterpret_cast<LPVOID>(ModUtils::SigScan(signature, Msg, true));
        if (!lpDestination)
        {
            ModUtils::RaiseError("Cannot find signature for memory replacement: " + Msg);
        }
        else
        {
            lpDestination = LPBYTE(lpDestination) + offset;
        }
    }

    std::string GetName() { return Msg; }

    void Enable() override
    {
        if (IsEnabled()) { return; }

        lpBackup = MVirtualAlloc::Get().Alloc(Length);
        if (lpBackup)
        {
            ModUtils::MemCopy(uintptr_t(lpBackup), uintptr_t(lpDestination), Length);
        }
        else
        {
            ModUtils::RaiseError("Cannot allocate memory for: " + Msg);
        }

        ModUtils::MemSet(uintptr_t(lpDestination), 0x90, Length);
        ModUtils::MemCopy(uintptr_t(lpDestination), uintptr_t(Replacement.data()), Replacement.size());

        ModUtils::Log("(%s) replaced memory at %p", Msg.c_str(), lpDestination);

        UModSwitch::Enable();
    }

    void Disable() override
    {
        if (!IsEnabled()) { return; }

        if (!lpBackup)
        {
            return;
        }

        ModUtils::MemCopy(uintptr_t(lpDestination), uintptr_t(lpBackup), Length);

        ModUtils::Log("(%s) restored memory at %p", Msg.c_str(), lpDestination);

        UModSwitch::Disable();
    }
};

