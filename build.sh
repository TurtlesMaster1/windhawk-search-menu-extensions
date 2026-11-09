#!/bin/sh
# Assemble the single-file Windhawk mod and compile it (Windhawk flags) + a standalone test DLL.
set -e
cd "$(dirname "$0")"
python - <<'PY'
head=open('src/head.cpp',encoding='utf-8').read()
body=open('src/body.cpp',encoding='utf-8').read()
abi=open('src/pal_abi_min.h',encoding='utf-8').read()
body2=open('src/body2.cpp',encoding='utf-8').read()
open('search-menu-extensions.wh.cpp','w',encoding='utf-8',newline='\n').write(head+body.replace('//@@ABI@@',abi)+body2)
PY
CXX="/c/Program Files/Windhawk/Compiler/bin/clang++.exe"
mkdir -p out
FLAGS="-std=c++23 -O2 -shared -DUNICODE -D_UNICODE -D__USE_MINGW_ANSI_STDIO=0 -target x86_64-w64-mingw32"
LIBS="-lole32 -loleaut32 -lruntimeobject -lshlwapi -lshell32 -luuid -ladvapi32"
if [ "$1" != "test" ]; then
# Exactly what the Windhawk editor runs (UI/resources/app/extensions/windhawk/dist/extension.js, compileModInternal)
WHFLAGS="-std=c++23 -O2 -shared -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 -DNTDDI_VERSION=0x0A000008 -D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD"
"$CXX" $WHFLAGS '-DWH_MOD_ID=L"search-menu-extensions"' '-DWH_MOD_VERSION=L"0.1.0"' "/c/Program Files/Windhawk/Engine/1.7.3/64/windhawk.lib" -x c++ search-menu-extensions.wh.cpp -include windhawk_api.h -target x86_64-w64-mingw32 -Wl,--export-all-symbols -o out/mod.dll $LIBS 2>&1 | grep -E 'error' | head -40 || true
# The editor's warning pass
"$CXX" $WHFLAGS -DWH_EDITING -Wall -Wextra -Wno-cast-function-type-mismatch -Wno-missing-field-initializers -Wno-unused-parameter -fsyntax-only -x c++ search-menu-extensions.wh.cpp -include windhawk_api.h -target x86_64-w64-mingw32 2>&1 | grep -E 'warning|error' | head -40 || true
fi
cat src/test_shim.h search-menu-extensions.wh.cpp src/test_main.cpp > out/test-build.cpp
"$CXX" $FLAGS -x c++ out/test-build.cpp -o out/${TESTDLL:-wse-test.dll} -static $LIBS 2>&1 | grep -E 'error' | head -40 || true
"$CXX" --target=x86_64-w64-mingw32 -municode -std=c++23 -O2 -static test/inject.cpp -o out/inject.exe 2>&1 | grep -E error | head -20 || true
ls -la out
