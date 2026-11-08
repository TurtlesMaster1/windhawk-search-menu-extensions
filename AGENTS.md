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
  compiles), the standalone test DLL `wse-test.dll` (same source without `WH_MOD`), and
  `inject.exe`. `./build.sh test` skips `mod.dll`. If the test DLL is locked because it's loaded
  in the composer, use `TESTDLL=wse-test2.dll ./build.sh test`.
- The compiler is Windhawk's bundled clang (`C:\Program Files\Windhawk\Compiler`). Keep the code
  compiling with `-std=c++23` under llvm-mingw. No MSVC-only constructs, no WRL/C++/WinRT
  headers: the WinRT ABI is hand-written in `pal_abi_min.h` (regenerate with `src/gen_abi.py`
  from `winmdidl` output of `C:\Windows\System32\WinMetadata\Windows.UI.winmd`).
- Keep the public C API (`WSE_*`, `WSE_ProviderCallbacks`) and the pipe protocol
  backward-compatible. Add fields at the end of structs and gate them on `cbSize`. Bump
  `WSE_GetApiVersion` only for additive changes, and document them in the readme in `head.cpp`.
- The vtable slot numbers in `body.cpp` (IPaletteCatalogStatics, IPaletteCatalog,
  IPaletteCatalogPrivate) are build-specific. If a new Insider build breaks things, re-derive the
  slots from the winmd and `OSClient.API.dll` before changing logic. Also update the
  "Updated for Build ..." banner at the top of both readmes.

## Testing without the Windhawk engine

1. `./build.sh test`
2. Open the new search once so the composer process exists, then run
   `test/restart-inject.ps1 -dlls <full path to out/wse-test.dll>`. It restarts the composer
   and injects early, because Windows only reads providers at startup. The early `LoadLibrary`
   sometimes fails, so just run it again.
3. `test/uiasearch.ps1 -text "wh"` types into search through UI Automation and dumps the result
   list. `-item`, `-how`, `-button` and `-shot` act on a row or take a screenshot. Use UIA, not
   synthetic keys: an elevated window with focus blocks SendInput.
4. The log is `%TEMP%\wse-test.log`. `out/inject.exe <pid> <dll> uninit` unloads the DLL.

When you're done, restart the composer (kill the `shellhost.exe` whose command line contains
`SystemComposerControl`) so no test DLLs stay loaded.

## Known limits (don't spend time on them unless asked)

- Adaptive Card `Action.Submit` buttons are ignored in the inline preview. RunUX logs "Slot
  previews do not support card actions".
- A synchronous pipe handle deadlocks if a client writes while another thread reads (that's
  Windows behaviour). The examples answer on the reading thread.
