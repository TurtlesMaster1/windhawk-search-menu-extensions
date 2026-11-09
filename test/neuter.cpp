// Test helper: stops an installed copy of this mod inside the process it's injected into, so the
// test DLL can be tried in a composer that Windhawk also loaded the mod into. It calls the copy's
// exported Wh_ModUninit (Windhawk builds export every symbol). Only affects this process; the
// installed mod is back the next time the composer starts.
// Build: see build.sh. Use: inject.exe <pid> out\neuter.dll init (before the test DLL).
#include <windows.h>
#include <psapi.h>
#include <stdio.h>

static void Log(const wchar_t* msg, const wchar_t* arg) {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"wse-test.log");
    if (FILE* f = _wfopen(path, L"a, ccs=UTF-8")) {
        fwprintf(f, L"[neuter] %s %s\n", msg, arg ? arg : L"");
        fclose(f);
    }
}

extern "C" __declspec(dllexport) DWORD WINAPI TestInit(void*) {
    for (int tries = 0; tries < 60; tries++) {  // the engine may still be loading it
        HMODULE mods[1024];
        DWORD needed = 0;
        EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed);
        for (DWORD i = 0; i < needed / sizeof(HMODULE); i++) {
            wchar_t name[MAX_PATH];
            if (!GetModuleBaseNameW(GetCurrentProcess(), mods[i], name, MAX_PATH)) continue;
            if (!wcsstr(name, L"search-menu-extensions_")) continue;
            auto un = (void (*)())GetProcAddress(mods[i], "_Z12Wh_ModUninitv");
            if (!un) un = (void (*)())GetProcAddress(mods[i], "Wh_ModUninit");
            if (!un) {
                Log(L"no Wh_ModUninit in", name);
                return 1;
            }
            Sleep(100);  // let its Wh_ModInit finish
            un();
            Log(L"stopped installed copy", name);
            return 0;
        }
        Sleep(50);
    }
    Log(L"no installed copy found", nullptr);
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI TestUninit(void*) { return 0; }
