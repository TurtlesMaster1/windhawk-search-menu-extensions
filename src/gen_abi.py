"""Generate compact raw-ABI C++ declarations for Windows.UI.Palettes from winmdidl output."""
import re, sys

src = open('idl/Windows.UI.Palettes.idl', encoding='utf-8', errors='replace').read()

# runtimeclass -> default interface
default_iface = {}
for m in re.finditer(r'runtimeclass\s+(\w+)(?:\s*:\s*[\w.]+)?\s*\{([^}]*)\}', src):
    d = re.search(r'\[default\]\s*interface\s+Windows\.UI\.Palettes\.(\w+)', m.group(2))
    if d:
        default_iface[m.group(1)] = d.group(1)

ifaces = []
for m in re.finditer(r'\[uuid\(([0-9A-Fa-f-]+)\)\]\s*interface\s+(\w+)\s*:\s*IInspectable\s*\{(.*?)\n\s*\}', src, re.S):
    ifaces.append((m.group(2), m.group(1), m.group(3)))
delegates = []
for m in re.finditer(r'\[uuid\(([0-9A-Fa-f-]+)\)\]\s*delegate\s*HRESULT\s+(\w+)\((.*?)\);', src, re.S):
    delegates.append((m.group(2), m.group(1), m.group(3)))

PRIM = {'HSTRING': 'HSTRING', 'boolean': 'boolean', 'UINT32': 'UINT32', 'INT32': 'INT32', 'FLOAT': 'FLOAT',
        'UINT16': 'UINT16', 'GUID': 'GUID', 'EventRegistrationToken': 'INT64',
        'Windows.Foundation.DateTime': 'INT64'}

def ctype(t):
    t = re.sub(r'<.*>', '', t.strip())
    stars = t.count('*')
    base = t.replace('*', '').strip()
    if base.startswith('Windows.UI.Palettes.'):
        n = base.split('.')[-1]
        if n in default_iface:
            n = default_iface[n]
        if n.startswith('I') or n in [d[0] for d in delegates]:
            return 'Pal::' + n + '*' * stars
    if base in PRIM:
        return PRIM[base] + '*' * stars
    if base.startswith('Windows.UI.Palettes.'):  # enums
        return 'INT32' + '*' * stars
    return 'IInspectable' + '*' * stars  # any other WinRT interface

def params(ps):
    out = []
    for p in re.split(r',\s*(?=\[)', ps.strip()):
        if not p:
            continue
        p = re.sub(r'\[[^\]]*\]\s*', '', p).strip()
        mm = re.match(r'(.*?)(\w+)$', p)
        out.append(f'{ctype(mm.group(1))} {mm.group(2)}')
    return ', '.join(out)

def uuid_init(u):
    a = u.split('-')
    b = a[3] + a[4]
    bytes_ = ', '.join('0x' + b[i:i + 2] for i in range(0, 16, 2))
    return f'{{0x{a[0]}, 0x{a[1]}, 0x{a[2]}, {{{bytes_}}}}}'

out = ['// ---- generated from Windows.UI.winmd (Windows.UI.Palettes) ----', 'namespace Pal {']
for n, _, _ in ifaces:
    out.append(f'struct {n};')
for n, _, _ in delegates:
    out.append(f'struct {n};')
for n, u, ps in delegates:
    out.append(f'[[maybe_unused]] constexpr GUID IID_{n} = {uuid_init(u)};')
    out.append(f'struct {n} : IUnknown {{ virtual HRESULT STDMETHODCALLTYPE Invoke({params(ps)}) = 0; }};')
want = set(sys.argv[1:]) if len(sys.argv) > 1 else None
for n, u, body in ifaces:
    if want and n not in want:
        continue
    out.append(f'[[maybe_unused]] constexpr GUID IID_{n} = {uuid_init(u)};')
    out.append(f'struct {n} : IInspectable {{')
    for mm in re.finditer(r'((?:\[[^\]]*\]\s*)*)HRESULT\s+(\w+)\((.*?)\);', body, re.S):
        attrs, name, ps = mm.groups()
        pre = 'get_' if 'propget' in attrs else 'put_' if 'propput' in attrs else 'add_' if 'eventadd' in attrs else 'remove_' if 'eventremove' in attrs else ''
        out.append(f'  virtual HRESULT STDMETHODCALLTYPE {pre}{name}({params(ps)}) = 0;')
    out.append('};')
out.append('} // namespace Pal')
print('\n'.join(out))
