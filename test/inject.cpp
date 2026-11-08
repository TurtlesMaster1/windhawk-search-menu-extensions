// Test helper: inject wse-test.dll into a process and call an export on it.
//   inject <pid> <dll> init|uninit
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include <initializer_list>

static HMODULE RemoteModule(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    MODULEENTRY32W me{sizeof(me)};
    HMODULE r = nullptr;
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
        if (!_wcsicmp(me.szModule, name)) r = me.hModule;
    CloseHandle(snap);
    return r;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) return 2;
    DWORD pid = wcstoul(argv[1], nullptr, 10);
    const wchar_t* dll = argv[2];
    const wchar_t* what = argv[3];
    HANDLE p = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!p) { printf("OpenProcess %lu\n", GetLastError()); return 1; }
    const wchar_t* base = wcsrchr(dll, L'\\') ? wcsrchr(dll, L'\\') + 1 : dll;
    HMODULE rm = RemoteModule(pid, base);
    if (!rm) {
        size_t sz = (wcslen(dll) + 1) * 2;
        void* mem = VirtualAllocEx(p, nullptr, sz, MEM_COMMIT, PAGE_READWRITE);
        WriteProcessMemory(p, mem, dll, sz, nullptr);
        HANDLE t = CreateRemoteThread(p, nullptr, 0,
            (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"), mem, 0, nullptr);
        if (!t) { printf("CreateRemoteThread %lu\n", GetLastError()); return 1; }
        WaitForSingleObject(t, 10000);
        DWORD code = 0;
        GetExitCodeThread(t, &code);
        CloseHandle(t);
        if (!code) {
            // Re-run through a stub that returns GetLastError() so we learn why.
            unsigned char stub[64];
            int k = 0;
            auto emit = [&](std::initializer_list<unsigned char> b) { for (auto x : b) stub[k++] = x; };
            auto imm64 = [&](void* v) { memcpy(stub + k, &v, 8); k += 8; };
            emit({0x48, 0x83, 0xEC, 0x28});                     // sub rsp,28h
            emit({0x48, 0xB9}); imm64(mem);                     // mov rcx, path
            emit({0x48, 0xB8}); imm64((void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
            emit({0xFF, 0xD0});                                 // call rax
            emit({0x48, 0xB8}); imm64((void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetLastError"));
            emit({0xFF, 0xD0});                                 // call rax
            emit({0x48, 0x83, 0xC4, 0x28, 0xC3});               // add rsp,28h; ret
            void* rs = VirtualAllocEx(p, nullptr, sizeof(stub), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            WriteProcessMemory(p, rs, stub, k, nullptr);
            HANDLE t2 = CreateRemoteThread(p, nullptr, 0, (LPTHREAD_START_ROUTINE)rs, nullptr, 0, nullptr);
            DWORD err = 0;
            if (t2) { WaitForSingleObject(t2, 10000); GetExitCodeThread(t2, &err); CloseHandle(t2); }
            printf("LoadLibrary returned NULL (remote GetLastError=%lu)\n", err);
            return 1;
        }
        for (int i = 0; i < 100 && !rm; i++) {
            rm = RemoteModule(pid, base);
            if (!rm) Sleep(20);
        }
        if (!rm) { printf("module not found after load (err=%lu)\n", GetLastError()); return 1; }
        printf("loaded at %p\n", rm);
    }
    HMODULE lm = LoadLibraryExW(dll, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    const char* exp = !wcscmp(what, L"init") ? "TestInit" : "TestUninit";
    FARPROC f = GetProcAddress(lm, exp);
    if (!f) { printf("no export %s\n", exp); return 1; }
    auto remote = (LPTHREAD_START_ROUTINE)((BYTE*)rm + ((BYTE*)f - (BYTE*)lm));
    HANDLE t = CreateRemoteThread(p, nullptr, 0, remote, nullptr, 0, nullptr);
    if (!t) { printf("CreateRemoteThread(%s) %lu\n", exp, GetLastError()); return 1; }
    DWORD w = WaitForSingleObject(t, 20000);
    printf("%s done (wait=%lu)\n", exp, w);
    return 0;
}
