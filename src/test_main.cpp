// Appended to the mod source for the standalone test build only (see build.sh). Mirrors the
// engine's order: Wh_ModInit, apply hooks / Wh_ModBeforeUninit, remove hooks, Wh_ModUninit.
extern "C" __declspec(dllexport) DWORD WINAPI TestInit(void*) {
    if (Wh_ModInit()) TestApplyHooks();
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI TestUninit(void*) {
    Wh_ModBeforeUninit();
    TestRemoveHooks();
    Wh_ModUninit();
    return 0;
}
