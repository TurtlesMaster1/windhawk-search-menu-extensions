// Appended to the mod source for the standalone test build only (see build.sh).
extern "C" __declspec(dllexport) DWORD WINAPI TestInit(void*) {
    LibInit();
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI TestUninit(void*) {
    LibUninit();
    return 0;
}
