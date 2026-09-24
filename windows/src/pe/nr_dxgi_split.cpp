// dxgi-original.dll on the OptiScaler route: DXVK's DXGI for the game, the system's for the
// graphics driver.
//
// OptiScaler is dxgi.dll there and loads its "original" DXGI as dxgi-original.dll, which was DXVK's
// dxgi renamed - the game has to run on DXVK for the network to share its Vulkan device. But
// OptiScaler also answers *every* load of a file called dxgi.dll with itself, and the AMD Windows
// Vulkan driver presents through DXGI: at the first present amdvlk64.dll loads
// C:\Windows\system32\dxgi.dll by full path, creates a D3D12 device through amdxc64.dll and asks a
// DXGI factory for a swap chain on its own D3D12 queue. Through OptiScaler that factory was DXVK's,
// which does not know a native D3D12 queue ("CreateSwapChainForHwnd: Unsupported device type"), so
// vkCreateSwapchainKHR failed with VK_ERROR_FORMAT_NOT_SUPPORTED and DXVK retried forever - 7 Days
// to Die ran with sound and never showed a frame (package test 3). RADV presents without DXGI, so
// Linux never met this.
//
// This module takes dxgi-original.dll's place and DXVK's dxgi becomes dxgi-dxvk.dll beside it. Each
// export looks at its call stack: if a module from the driver store (System32\DriverStore, where
// every installed display driver lives) is on it, the call is the driver's own presentation and goes
// to the system DXGI; everything else goes to DXVK, as before. OptiScaler is not modified and still
// sees every call first.
//
// The system DXGI is loaded in DllMain, by full path. OptiScaler loads this module while it works out
// its mode, before it hooks the loader (unless its EarlyHooking is on), so that load reaches the real
// file; later, its hook compares the tail of every requested name with "dxgi.dll" and hands back
// itself. If the early load did not produce the system file, a second try uses "...\dxgi.dll." (a
// name the hook does not recognise; Windows drops the trailing dot, Wine does not - measured under
// GE-Proton 11-7, where it fails with 126) and otherwise the driver's calls stay on DXVK, as before.
#include "nr_pe_log.hpp"

#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <string>

namespace {

using nr::pe::log;

HMODULE self = nullptr;
HMODULE dxvk = nullptr, system_dxgi = nullptr;
INIT_ONCE dxvk_once = INIT_ONCE_STATIC_INIT, system_once = INIT_ONCE_STATIC_INIT;
std::wstring driver_store;  // lower case, with the trailing backslash

std::wstring lower(std::wstring s) {
    for (auto& c : s) c = wchar_t(towlower(c));
    return s;
}

std::wstring folder_of(HMODULE module) {
    wchar_t path[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring s(path, n);
    const auto slash = s.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring{} : s.substr(0, slash + 1);
}

BOOL CALLBACK load_dxvk(PINIT_ONCE, PVOID, PVOID*) {
    const std::wstring path = folder_of(self) + L"dxgi-dxvk.dll";
    dxvk = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (dxvk) log("[nr] dxgi split: DXVK's DXGI from dxgi-dxvk.dll");
    else log("[nr] dxgi split: dxgi-dxvk.dll failed to load (error %lu)", GetLastError());
    return TRUE;
}

std::wstring system_dir() {
    wchar_t dir[MAX_PATH]{};
    const UINT n = GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir, n) + L"\\";
}

// The module only if it really is System32's dxgi.dll - not OptiScaler answering for the name.
HMODULE checked_system(HMODULE m, const char* how) {
    const DWORD error = m ? 0 : GetLastError();
    wchar_t got[MAX_PATH]{};
    const DWORD n = m ? GetModuleFileNameW(m, got, MAX_PATH) : 0;
    const bool ok = m && m != self && lower(std::wstring(got, n)) == lower(system_dir() + L"dxgi.dll");
    log("[nr] dxgi split: system DXGI %s: %ls%s (error %lu)", how, m ? got : L"not loaded",
        m && !ok ? " - not the system file" : "", error);
    return ok ? m : nullptr;
}

BOOL CALLBACK load_system(PINIT_ONCE, PVOID, PVOID*) {
    if (!system_dxgi)
        system_dxgi = checked_system(LoadLibraryExW((system_dir() + L"dxgi.dll.").c_str(), nullptr, 0),
                                     "second try");
    if (!system_dxgi) log("[nr] dxgi split: the driver's calls stay on DXVK");
    return TRUE;
}

// Is a display driver on this thread's stack? The walk stops at frames without unwind data, which
// driver and system modules all have.
bool from_driver(const char** who) {
    void* frames[62];
    const USHORT n = RtlCaptureStackBackTrace(1, 62, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        HMODULE m = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                static_cast<LPCWSTR>(frames[i]), &m) || !m || m == self)
            continue;
        wchar_t path[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(m, path, MAX_PATH);
        const std::wstring p = lower(std::wstring(path, len));
        if (!driver_store.empty() && p.compare(0, driver_store.size(), driver_store) == 0) {
            static thread_local char name[MAX_PATH];
            const auto slash = p.find_last_of(L'\\');
            std::snprintf(name, sizeof name, "%ls", p.c_str() + (slash == std::wstring::npos ? 0 : slash + 1));
            *who = name;
            return true;
        }
    }
    return false;
}

FARPROC target(const char* name) {
    const char* who = nullptr;
    if (from_driver(&who)) {
        InitOnceExecuteOnce(&system_once, load_system, nullptr, nullptr);
        if (system_dxgi) {
            static LONG reported = 0;
            if (InterlockedIncrement(&reported) <= 8)
                log("[nr] dxgi split: %s from %s -> system DXGI", name, who);
            return GetProcAddress(system_dxgi, name);
        }
    }
    InitOnceExecuteOnce(&dxvk_once, load_dxvk, nullptr, nullptr);
    return dxvk ? GetProcAddress(dxvk, name) : nullptr;
}

}  // namespace

extern "C" {

__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** factory) {
    using Fn = HRESULT(WINAPI*)(REFIID, void**);
    const auto fn = reinterpret_cast<Fn>(target("CreateDXGIFactory"));
    return fn ? fn(riid, factory) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** factory) {
    using Fn = HRESULT(WINAPI*)(REFIID, void**);
    const auto fn = reinterpret_cast<Fn>(target("CreateDXGIFactory1"));
    return fn ? fn(riid, factory) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, void** factory) {
    using Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto fn = reinterpret_cast<Fn>(target("CreateDXGIFactory2"));
    return fn ? fn(flags, riid, factory) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport(void) {
    using Fn = HRESULT(WINAPI*)();
    const auto fn = reinterpret_cast<Fn>(target("DXGIDeclareAdapterRemovalSupport"));
    return fn ? fn() : S_OK;
}

__declspec(dllexport) HRESULT WINAPI DXGIGetDebugInterface1(UINT flags, REFIID riid, void** debug) {
    using Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto fn = reinterpret_cast<Fn>(target("DXGIGetDebugInterface1"));
    return fn ? fn(flags, riid, debug) : E_NOINTERFACE;
}

}  // extern "C"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self = instance;
        nr::pe::set_module(instance);
        DisableThreadLibraryCalls(instance);
        driver_store = lower(system_dir() + L"DriverStore\\");
        system_dxgi = checked_system(LoadLibraryExW((system_dir() + L"dxgi.dll").c_str(), nullptr, 0),
                                     "at load");
    }
    return TRUE;
}
