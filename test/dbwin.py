# Capture OutputDebugString output (where Windhawk mod logs go) from processes in this session.
# Usage: python test/dbwin.py <outfile> [filter]
import ctypes
import ctypes.wintypes as wt
import sys

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.CreateEventW.restype = wt.HANDLE
k32.CreateFileMappingW.restype = wt.HANDLE
k32.MapViewOfFile.restype = ctypes.c_void_p
k32.MapViewOfFile.argtypes = [wt.HANDLE, wt.DWORD, wt.DWORD, wt.DWORD, ctypes.c_size_t]

out = open(sys.argv[1], 'a', encoding='utf-8', buffering=1)
flt = sys.argv[2].lower() if len(sys.argv) > 2 else ''

ready = k32.CreateEventW(None, False, True, 'DBWIN_BUFFER_READY')
data = k32.CreateEventW(None, False, False, 'DBWIN_DATA_READY')
mapping = k32.CreateFileMappingW(wt.HANDLE(-1), None, 0x04, 0, 4096, 'DBWIN_BUFFER')
view = k32.MapViewOfFile(mapping, 0x0004, 0, 0, 4096)
if not (ready and data and mapping and view):
    sys.exit('could not open DBWIN objects (another debug listener running?) err=%d' % ctypes.get_last_error())
out.write('--- listening\n')
while True:
    k32.SetEvent(ready)
    if k32.WaitForSingleObject(data, 0xFFFFFFFF) != 0:
        continue
    pid = ctypes.c_uint32.from_address(view).value
    msg = ctypes.string_at(view + 4).decode('utf-8', 'replace').rstrip()
    if not flt or flt in msg.lower():
        out.write('[%d] %s\n' % (pid, msg))
