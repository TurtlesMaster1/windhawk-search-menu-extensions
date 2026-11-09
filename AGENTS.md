# Notes for coding agents

This repo is a Windhawk library mod that injects items into the **new Windows 11 Insider
"composer" search** (`shellhost.exe loadComponent SystemComposerControl`). Read `README.md`
first; this file covers how to work on it safely.

## Ground rules

- **Never edit `search-menu-extensions.wh.cpp` by hand.** It's generated. Edit `src/head.cpp`
  (Windhawk header, readme, settings), `src/body.cpp` (core) or `src/body2.cpp` (C API, pipe,
  demo, lifetime), then run `./build.sh` (Git Bash). `//@@ABI@@` in `body.cpp` is replaced by
  `src/pal_abi_min.h`.
- `./build.sh` builds three things into `out/`: `mod.dll` (real Windhawk flags, proves the mod
  compiles), the standalone test DLL `wse-test.dll`, and `inject.exe`.
  The test DLL is the mod source with `src/test_shim.h` prepended and `src/test_main.cpp`
  appended. Keep all test-only code in those two files: the windhawk-mods validator flags
  `WH_MOD` checks and `#define Wh_...` redefinitions in the published mod. `./build.sh test` skips `mod.dll`. If the test DLL is locked because it's loaded
  in the composer, use `TESTDLL=wse-test2.dll ./build.sh test`.
- The compiler is Windhawk's bundled clang (`C:\Program Files\Windhawk\Compiler`). Keep the code
  compiling with `-std=c++23` under llvm-mingw. No MSVC-only constructs. The C++/WinRT headers
  Windhawk ships are fine (the code uses `winrt/Windows.Data.Json.h`), but there's no projection
  for `Windows.UI.Palettes`, so its ABI is hand-written in `pal_abi_min.h` (regenerate with
  `src/gen_abi.py` from `winmdidl` output of `C:\Windows\System32\WinMetadata\Windows.UI.winmd`).
- Keep the public C API (`WSE_*`, `WSE_ProviderCallbacks`) and the pipe protocol
  backward-compatible. Add fields at the end of structs and gate them on `cbSize`. Bump
  `WSE_GetApiVersion` only for additive changes, and document them in the readme in `head.cpp`.
- The vtable slot numbers in `body.cpp` (IPaletteCatalogStatics, IPaletteCatalog,
  IPaletteCatalogPrivate) are build-specific. If a new Insider build breaks things, re-derive the
  slots from the winmd and `OSClient.API.dll` before changing logic. Also update the
  "Updated for Build ..." banner at the top of both readmes.

## Testing without the Windhawk engine

The test DLL emulates the engine: `Wh_SetFunctionHook` there patches every pointer to the target
in `OSClient.API.dll`'s vtables, and `TestInit`/`TestUninit` call `Wh_ModInit`, then
`Wh_ModBeforeUninit` and `Wh_ModUninit` in the engine's order. An installed copy of the mod (Windhawk
loads it into the composer too) answers the queries and patches the same functions: disable it, or
inject `out/neuter.dll` first (`restart-inject.ps1 -dlls <neuter.dll>,<wse-test.dll>`), which calls
that copy's exported `Wh_ModUninit` in the test composer only. Hooks are applied after injection, so
if the log says "raising PaletteCatalog.Changed to 1 subscribers" Windows subscribed before the
hooks: live registration won't show up, so restart-inject again until it says 2.

1. `./build.sh test`
2. Open the new search once so the composer process exists, then run
   `test/restart-inject.ps1 -dlls <full path to out/wse-test.dll>`. It restarts the composer
   and injects early, because Windows only reads providers at startup. The early `LoadLibrary`
   sometimes fails, so just run it again.
3. `test/uiasearch.ps1 -text "wh"` types into search through UI Automation and dumps the result
   list. `-item`, `-how`, `-button` and `-shot` act on a row or take a screenshot. Use UIA, not
   synthetic keys: an elevated window with focus blocks SendInput.
4. The log is `%TEMP%\wse-test.log`. `out/inject.exe <pid> <dll> uninit` unloads the DLL.

Under the real engine: `test/install-dev.ps1` (from an elevated PowerShell) installs `out/mod.dll`
as the local mod with logging on, and `python test/dbwin.py <logfile> search-menu` captures the log.

When you're done, restart the composer (kill the `shellhost.exe` whose command line contains
`SystemComposerControl`) so no test DLLs stay loaded.

## Known limits (don't spend time on them unless asked)

- Adaptive Card `Action.Submit` buttons are ignored in the inline preview. RunUX logs "Slot
  previews do not support card actions".
- A synchronous pipe handle deadlocks if a client writes while another thread reads (that's
  Windows behaviour). The examples answer on the reading thread.
