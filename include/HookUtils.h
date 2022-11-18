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

    void Scan()
    {
        lpScan = lpScan ? lpScan : (LPBYTE)baseAddress;
        SIZE_T numBytes = VirtualQuery(lpScan, &memInfo, sizeof(memInfo));

        while (numBytes)
        {
            lpScan = static_cast<LPBYTE>(memInfo.BaseAddress);

            if (memInfo.State == MEM_FREE)
            {
                break;
            }

            lpScan -= sys.dwAllocationGranularity;
            numBytes = VirtualQuery(lpScan, &memInfo, sizeof(memInfo));
        }
    }

public:

    // Allocate memory below process base address
    // This allows for memory hooks using shorter jump ops (eg. 0xE9)
    // Subsequent calls usually access the same memory page
    // see VirtualAlloc function
    // 
    // dwSize in [1..4096]
    LPVOID Alloc(SIZE_T dwSize, DWORD flAllocType = MEM_RESERVE | MEM_COMMIT, DWORD flProtec = PAGE_EXECUTE_READWRITE)
    {
        if (dwSize > 0x1000)
        {
            return nullptr;
        }

        bytesAllocated += static_cast<DWORD>(dwSize);

        if (!lpCurrent || bytesAllocated > currentPageSize)
        {
            Scan();
            ModUtils::Log("Allocating page at: %p", lpScan);

            // Preallocate a region equals to system page size (typically 4KiB)
            lpCurrent = VirtualAlloc(lpScan, sys.dwPageSize, flAllocType, flProtec);

            bytesAllocated = static_cast<DWORD>(dwSize);
            currentPageSize = static_cast<DWORD>(sys.dwPageSize);
        }

        return (LPBYTE)lpCurrent + bytesAllocated - dwSize;
    }

    // there's no deallocation 'cause we don't need it, for now
};

// writes an absolute jump to destination at specified address (14 bytes)
class UHookAbsoluteNoCopy
{
    static const uint16_t op = 0x25ff;

    LPVOID lpHook;
    LPVOID lpDest;

    UHookAbsoluteNoCopy(UHookAbsoluteNoCopy&) = delete;

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
};

// Creates or removes a hook using a relative jump (5 bytes)
// First jump to an intermediate address at which we then do an absolute jump to the custom code
// This way we don't have to determine the size of the custom asm
// We also copy the stolen bytes over to the intermediate location so the custom code can omit the original code
class UHookRelativeIntermediate
{
public:
    static const uint8_t op = 0xE8;
    static const unsigned char opSize = 5;

private:

    LPVOID lpHook = nullptr;
    LPVOID lpIntermediate = nullptr;
    LPVOID lpDestination = nullptr;
    size_t numBytes = 5;
    MVirtualAlloc& allocator = MVirtualAlloc::Get();

    bool bCanHook = false;
    bool bEnabled = false;
    std::unique_ptr<UHookAbsoluteNoCopy> upJmpAbs;

    // Initialize the intermediate code that we can decide to jump to later
    void Init()
    {
        lpIntermediate = allocator.Alloc(numBytes + 14 + rspUp.size() + rspDown.size()); // one 14B jump, two 3B adds

        // move stack pointer up so stolen instructions can access the stack
        ModUtils::MemCopy(uintptr_t(lpIntermediate), uintptr_t(rspUp.data()), rspUp.size());

        // copy to be stolen bytes to the imtermediate location
        ModUtils::MemCopy(reinterpret_cast<uint64_t>(lpIntermediate) + rspUp.size(), reinterpret_cast<uint64_t>(lpHook), numBytes);

        // move stack pointer down so the custom code can return
        ModUtils::MemCopy(uintptr_t(lpIntermediate) + rspUp.size() + numBytes, uintptr_t(rspDown.data()), rspDown.size());

        // create jump from intermediate code to custom code
        upJmpAbs = std::make_unique<UHookAbsoluteNoCopy>(lpIntermediate, lpDestination, rspUp.size() + numBytes + rspDown.size());
        upJmpAbs->Enable();

        ModUtils::Log("Generated hook '%s' from %p to %p at %p", msg.c_str(), lpHook, lpDestination, lpIntermediate);
    }

public:
    const std::string msg;

    UHookRelativeIntermediate(UHookRelativeIntermediate&) = delete;
    UHookRelativeIntermediate(
        std::vector<uint16_t> signature,
        size_t numStolenBytes,
        LPVOID destination,
        int offset = 0,
        std::string msg = "Unknown Hook",
        std::function<void()> enable = []() {},
        std::function<void()> disable = []() {}
    )
        : numBytes(numStolenBytes), lpDestination(destination), msg(msg), fnEnable(enable), fnDisable(disable)
    {
        lpHook = reinterpret_cast<LPVOID>(ModUtils::SigScan(signature, false, msg, true) + offset);
        bCanHook = lpHook != nullptr;
        //Init();
    }

    const bool HasFoundSignature() const { return bCanHook; }

    static const std::vector<uint8_t> rspUp; // lea rsp,[rsp+8] (5B)
    static const std::vector<uint8_t> rspDown; // lea rsp,[rsp-8] (5B)

    std::function<void()> fnEnable;
    std::function<void()> fnDisable;

    void Enable()
    {
        if (!lpIntermediate)
        {
            Init();
        }

        if (!bCanHook || bEnabled) { return; }
        ModUtils::Log("Enabling hook '%s' from %p to %p", msg.c_str(), lpHook, lpDestination);

        // pad the jump in case numBytes > jump instruction size
        ModUtils::MemSet(reinterpret_cast<uintptr_t>(lpHook), 0x90, numBytes);

        // write instruction at hook address
        *static_cast<uint8_t*>(lpHook) = op;
        uint32_t relOffset = static_cast<uint32_t>(static_cast<uint8_t*>(lpIntermediate) - static_cast<uint8_t*>(lpHook) - opSize);
        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(lpHook) + 1) = relOffset;

        fnEnable();
        bEnabled = true;
    }
    void Disable()
    {
        if (!bEnabled) { return; }
        ModUtils::Log("Disabling hook '%s' from %p to %p", msg.c_str(), lpHook, lpDestination);
        ModUtils::MemCopy(uintptr_t(lpHook), uintptr_t(lpIntermediate) + rspUp.size(), numBytes);

        fnDisable();
        bEnabled = false;
    }
    void Toggle()
    {
        bEnabled ? Disable() : Enable();
    }
    ~UHookRelativeIntermediate() { Disable(); }
};

const std::vector<uint8_t> UHookRelativeIntermediate::rspUp({ 0x48, 0x8D, 0x64, 0x24, 0x08 });
const std::vector<uint8_t> UHookRelativeIntermediate::rspDown({ 0x48, 0x8D, 0x64, 0x24, 0xf8 });
