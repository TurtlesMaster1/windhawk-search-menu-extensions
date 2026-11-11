// ---------------------------------------------------------------------------
// Built-in providers: calculator + unit converter, kill process, custom commands
// ---------------------------------------------------------------------------
static std::wstring Lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(s.data(), (DWORD)s.size());
    return s;
}
static std::wstring Trim(std::wstring const& s) {
    size_t a = 0, b = s.size();
    while (a < b && s[a] == L' ') a++;
    while (b > a && s[b - 1] == L' ') b--;
    return s.substr(a, b - a);
}
static std::wstring Num(double v, int sig) {
    if (v == 0) v = 0;  // no "-0"
    wchar_t b[64];
    if (v == std::floor(v) && std::fabs(v) < 1e15)
        swprintf(b, 64, L"%.0f", v);
    else
        swprintf(b, 64, L"%.*g", sig, v);
    return b;
}

// Recursive-descent arithmetic: + - * / ^ ( ), "%" as percent or modulo, a few functions.
struct Calc {
    const wchar_t* p;
    bool ok = true;
    int ops = 0;    // operators and function calls seen; 0 means it was just a number
    int depth = 0;
    bool percent = false;  // the term just parsed was a plain "N%"

    void Ws() { while (*p == L' ') p++; }
    static bool StartsOperand(wchar_t c) { return iswalnum(c) || c == L'(' || c == L'.'; }
    double Expr() {
        double v = Term();
        for (;;) {
            Ws();
            bool minus = *p == L'-' || *p == 0x2212;
            if (*p != L'+' && !minus) return v;
            p++; ops++;
            double t = Term();
            if (percent) t *= v;  // "50 + 10%" is 55, like a pocket calculator
            v = minus ? v - t : v + t;
        }
    }
    double Term() {
        double v = Unary();
        percent = false;
        for (int n = 0;; n++) {
            Ws();
            wchar_t c = *p;
            if (c == L'*' || c == 0xD7) { p++; ops++; v *= Unary(); percent = false; }
            else if (c == L'/' || c == 0xF7) { p++; ops++; v /= Unary(); percent = false; }
            else if (c == L'%') {
                p++; ops++;
                const wchar_t* s = p;
                while (*s == L' ') s++;
                if (StartsOperand(*s)) { v = std::fmod(v, Unary()); percent = false; }  // "10 % 3"
                else { v /= 100; percent = n == 0; }               // "15%"
            } else return v;
        }
    }
    double Unary() {
        Ws();
        if (++depth > 64) { ok = false; return 0; }
        double v;
        if (*p == L'-' || *p == 0x2212) { p++; v = -Unary(); }
        else if (*p == L'+') { p++; v = Unary(); }
        else {
            v = Primary();
            Ws();
            if (*p == L'^') { p++; ops++; v = std::pow(v, Unary()); }
        }
        depth--;
        return v;
    }
    double Primary() {
        Ws();
        if (*p == L'(') {
            p++;
            double v = Expr();
            Ws();
            if (*p == L')') p++; else ok = false;
            return v;
        }
        if (iswdigit(*p) || *p == L'.') {
            wchar_t* end = nullptr;
            double v;
            if (p[0] == L'0' && (p[1] == L'x' || p[1] == L'X')) v = (double)wcstoull(p + 2, &end, 16);
            else v = wcstod(p, &end);
            if (!end || end == p || (end == p + 2 && p[1] != L'.' && !iswdigit(p[1]))) { ok = false; return 0; }
            p = end;
            return v;
        }
        std::wstring name;
        while (iswalpha(*p) || (!name.empty() && iswdigit(*p))) name += (wchar_t)towlower(*p++);
        if (name.empty()) { ok = false; return 0; }
        Ws();
        if (*p != L'(') {
            if (name == L"pi") return 3.14159265358979323846;
            if (name == L"e") return 2.71828182845904523536;
            ok = false;
            return 0;
        }
        p++; ops++;
        double a = Expr();
        Ws();
        if (*p == L')') p++; else ok = false;
        if (name == L"sqrt") return std::sqrt(a);
        if (name == L"cbrt") return std::cbrt(a);
        if (name == L"abs") return std::fabs(a);
        if (name == L"sin") return std::sin(a);
        if (name == L"cos") return std::cos(a);
        if (name == L"tan") return std::tan(a);
        if (name == L"asin") return std::asin(a);
        if (name == L"acos") return std::acos(a);
        if (name == L"atan") return std::atan(a);
        if (name == L"ln") return std::log(a);
        if (name == L"log") return std::log10(a);
        if (name == L"log2") return std::log2(a);
        if (name == L"exp") return std::exp(a);
        if (name == L"round") return std::round(a);
        if (name == L"floor") return std::floor(a);
        if (name == L"ceil") return std::ceil(a);
        ok = false;
        return 0;
    }
};

// needOps: a bare number isn't worth a result row.
static bool Evaluate(std::wstring const& text, bool needOps, double* out) {
    if (text.empty() || text.size() > 200) return false;
    Calc c{text.c_str()};
    double v = c.Expr();
    c.Ws();
    if (!c.ok || *c.p || (needOps && !c.ops) || !std::isfinite(v)) return false;
    *out = v;
    return true;
}

// names: space-separated aliases (matched ignoring case), the first is shown. cat 'T' (temperature): f is 1=C, 2=F, 3=K.
struct Unit { const wchar_t* names; char cat; double f; };
static const Unit kUnits[] = {
    {L"mm millimeter millimeters millimetre millimetres", 'L', 0.001},
    {L"cm centimeter centimeters centimetre centimetres", 'L', 0.01},
    {L"m meter meters metre metres", 'L', 1},
    {L"km kilometer kilometers kilometre kilometres", 'L', 1000},
    {L"in inch inches \"", 'L', 0.0254},
    {L"ft foot feet '", 'L', 0.3048},
    {L"yd yard yards", 'L', 0.9144},
    {L"mi mile miles", 'L', 1609.344},
    {L"nmi", 'L', 1852},
    {L"mg milligram milligrams", 'M', 1e-6},
    {L"g gram grams", 'M', 0.001},
    {L"kg kilogram kilograms kilo kilos", 'M', 1},
    {L"t tonne tonnes", 'M', 1000},
    {L"oz ounce ounces", 'M', 0.028349523125},
    {L"lb lbs pound pounds", 'M', 0.45359237},
    {L"st stone", 'M', 6.35029318},
    {L"ml milliliter milliliters millilitre millilitres", 'V', 0.001},
    {L"l liter liters litre litres", 'V', 1},
    {L"tsp teaspoon teaspoons", 'V', 0.00492892159375},
    {L"tbsp tablespoon tablespoons", 'V', 0.01478676478125},
    {L"floz", 'V', 0.0295735295625},
    {L"cups cup", 'V', 0.2365882365},
    {L"pt pint pints", 'V', 0.473176473},
    {L"qt quart quarts", 'V', 0.946352946},
    {L"gal gallon gallons", 'V', 3.785411784},
    {L"ms millisecond milliseconds", 'D', 0.001},
    {L"s sec secs second seconds", 'D', 1},
    {L"min mins minute minutes", 'D', 60},
    {L"h hr hrs hour hours", 'D', 3600},
    {L"d day days", 'D', 86400},
    {L"wk week weeks", 'D', 604800},
    {L"yr year years", 'D', 31557600},
    {L"m/s", 'S', 1},
    {L"km/h kmh kph", 'S', 1 / 3.6},
    {L"mph", 'S', 0.44704},
    {L"kn knot knots", 'S', 1852.0 / 3600},
    {L"ft/s", 'S', 0.3048},
    {L"bit bits", 'B', 0.125},
    {L"B byte bytes", 'B', 1},
    {L"KB", 'B', 1024.0},
    {L"MB", 'B', 1048576.0},
    {L"GB", 'B', 1073741824.0},
    {L"TB", 'B', 1099511627776.0},
    {L"\u00b0C c celsius", 'T', 1},
    {L"\u00b0F f fahrenheit", 'T', 2},
    {L"K kelvin", 'T', 3},
};

static const Unit* FindUnit(std::wstring const& name, std::wstring* shown) {
    for (auto& u : kUnits) {
        const wchar_t* s = u.names;
        while (*s) {
            const wchar_t* e = wcschr(s, L' ');
            size_t n = e ? (size_t)(e - s) : wcslen(s);
            if (n == name.size() && !_wcsnicmp(s, name.c_str(), n)) {
                const wchar_t* fe = wcschr(u.names, L' ');
                *shown = fe ? std::wstring(u.names, fe) : std::wstring(u.names);
                return &u;
            }
            s += n;
            while (*s == L' ') s++;
        }
    }
    return nullptr;
}

// "<number or expression> <unit> to|in <unit>"
static bool ConvertUnits(std::wstring const& text, std::wstring* title, std::wstring* value) {
    std::wstring t = Lower(text);
    size_t a = t.rfind(L" to "), b = t.rfind(L" in ");
    size_t pos = a == std::wstring::npos ? b : (b == std::wstring::npos ? a : std::max(a, b));
    if (pos == std::wstring::npos) return false;
    std::wstring left = Trim(t.substr(0, pos)), toName = Trim(t.substr(pos + 4));
    size_t i = left.size();
    while (i > 0 && !iswdigit(left[i - 1]) && left[i - 1] != L')' && left[i - 1] != L'.' && left[i - 1] != L' ') i--;
    std::wstring fromName = left.substr(i), num = Trim(left.substr(0, i));
    if (fromName.empty() || toName.empty() || num.empty()) return false;
    std::wstring fromShown, toShown;
    const Unit* from = FindUnit(fromName, &fromShown);
    const Unit* to = FindUnit(toName, &toShown);
    double x;
    if (!from || !to || from->cat != to->cat || !Evaluate(num, false, &x)) return false;
    double y;
    if (from->cat == 'T') {
        double k = from->f == 1 ? x + 273.15 : from->f == 2 ? (x - 32) * 5 / 9 + 273.15 : x;
        y = to->f == 1 ? k - 273.15 : to->f == 2 ? (k - 273.15) * 9 / 5 + 32 : k;
        if (std::fabs(y) < 1e-9) y = 0;
    } else {
        y = x * from->f / to->f;
    }
    *value = Num(y, 6);
    *title = Num(x, 10) + L" " + fromShown + L" = " + *value + L" " + toShown;
    return true;
}

static void CalcQuery(UINT64 q, std::wstring const& text) {
    std::wstring t = Trim(text), title, value;
    if (!t.empty() && t[0] == L'=') t = Trim(t.substr(1));
    double v;
    if (ConvertUnits(t, &title, &value)) {
    } else if (Evaluate(t, true, &v)) {
        value = Num(v, 12);
        title = L"= " + value;
    } else {
        SubmitResults(q, L"[]", TRUE);
        return;
    }
    SubmitResults(q,
                  L"[{\"id\":\"result\",\"title\":" + JsonEscape(title) + L",\"subtitle\":" +
                      JsonEscape(t + L" \u00b7 Enter to copy") +
                      L",\"icon\":{\"glyph\":\"\\uE8EF\"},\"score\":1.0,\"action\":{\"type\":\"copy\",\"text\":" +
                      JsonEscape(value) + L"}}]",
                  TRUE);
}

// "kill <part of a process name>": Enter ends that process.
static void KillQuery(UINT64 q, std::wstring const& text) {
    std::wstring t = Lower(Trim(text));
    if (t.rfind(L"kill", 0) != 0 || (t.size() > 4 && t[4] != L' ')) {
        SubmitResults(q, L"[]", TRUE);
        return;
    }
    std::wstring filter = Trim(t.substr(4));
    struct Proc { DWORD pid; std::wstring name, path; SIZE_T mem; };
    std::vector<Proc> procs;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{sizeof(pe)};
        for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
            if (pe.th32ProcessID <= 4) continue;
            if (!filter.empty() && !StrStrIW(pe.szExeFile, filter.c_str())) continue;
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!h) continue;  // not ours to end either
            Proc p{pe.th32ProcessID, pe.szExeFile, L"", 0};
            PROCESS_MEMORY_COUNTERS pmc{sizeof(pmc)};
            if (K32GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) p.mem = pmc.WorkingSetSize;
            wchar_t path[MAX_PATH];
            DWORD n = MAX_PATH;
            if (QueryFullProcessImageNameW(h, 0, path, &n)) p.path = path;
            CloseHandle(h);
            procs.push_back(std::move(p));
        }
        CloseHandle(snap);
    }
    std::sort(procs.begin(), procs.end(), [](Proc const& a, Proc const& b) { return a.mem > b.mem; });
    if (procs.size() > 8) procs.resize(8);
    std::wstring json = L"[";
    double score = 0.9;  // biggest first
    for (auto& p : procs) {
        wchar_t sub[128];
        swprintf(sub, 128, L"PID %lu \u00b7 %.0f MB \u00b7 Enter to end it", p.pid, p.mem / 1048576.0);
        if (json.size() > 1) json += L",";
        json += L"{\"id\":\"" + std::to_wstring(p.pid) + L"\",\"title\":" + JsonEscape(L"End " + p.name) +
                L",\"subtitle\":" + JsonEscape(sub) + L",\"icon\":" +
                (p.path.empty() ? std::wstring(L"{\"glyph\":\"\\uE71A\"}") : L"{\"path\":" + JsonEscape(p.path) + L"}") +
                L",\"score\":" + Num(score, 3) + L"}";
        score -= 0.01;
    }
    SubmitResults(q, json + L"]", TRUE);
}
static void KillInvoke(std::wstring const& item) {
    DWORD pid = wcstoul(item.c_str(), nullptr, 10);
    if (pid <= 4) return;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    BOOL ok = h && TerminateProcess(h, 1);
    if (h) CloseHandle(h);
    Wh_Log(L"kill %lu: %s", pid, ok ? L"ok" : L"failed");
}

// Custom commands from the settings: keyword -> program, file or URL. "{query}" stands for
// whatever is typed after the keyword.
struct CustomCommand { std::wstring keyword, title, command, args; };
static std::mutex g_commandsLock;
[[clang::no_destroy]] static std::vector<CustomCommand> g_commands;

static std::wstring UrlEncode(std::wstring const& s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string u(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), u.data(), n, nullptr, nullptr);
    std::wstring r;
    for (unsigned char c : u) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r += (wchar_t)c;
        else { wchar_t b[4]; swprintf(b, 4, L"%%%02X", c); r += b; }
    }
    return r;
}
static std::wstring FillQuery(std::wstring s, std::wstring const& query) {
    for (size_t i = 0; (i = s.find(L"{query}", i)) != std::wstring::npos; i += query.size()) s.replace(i, 7, query);
    return s;
}
static bool HasQuery(std::wstring const& s) { return s.find(L"{query}") != std::wstring::npos; }

static void CommandsQuery(UINT64 q, std::wstring const& text) {
    std::wstring t = Trim(text), lt = Lower(t), json = L"[";
    std::vector<CustomCommand> cmds;
    {
        std::lock_guard l(g_commandsLock);
        cmds = g_commands;
    }
    for (size_t i = 0; i < cmds.size() && !lt.empty(); i++) {
        auto& c = cmds[i];
        std::wstring kw = Lower(c.keyword), rest;
        if (HasQuery(c.command) || HasQuery(c.args)) {
            // "g cats": the keyword, then the text to pass on
            if (lt != kw && lt.rfind(kw + L" ", 0) != 0) continue;
            rest = Trim(t.substr(kw.size()));
        } else if (lt != kw && (lt.size() < 2 || (kw.rfind(lt, 0) != 0 && !StrStrIW(c.title.c_str(), lt.c_str())))) {
            continue;
        }
        bool url = c.command.find(L"://") != std::wstring::npos;
        std::wstring command = FillQuery(c.command, url ? UrlEncode(rest) : rest), args = FillQuery(c.args, rest);
        std::wstring title = FillQuery(c.title.empty() ? c.command : c.title, rest.empty() ? std::wstring(L"\u2026") : rest);
        if (json.size() > 1) json += L",";
        json += L"{\"id\":\"c" + std::to_wstring(i) + L"\",\"title\":" + JsonEscape(title) + L",\"subtitle\":" +
                JsonEscape(args.empty() ? command : command + L" " + args) + L",\"icon\":{\"glyph\":\"" +
                (url ? L"\\uE774" : L"\\uE756") + L"\"},\"score\":1.0,\"action\":{\"type\":\"run\",\"command\":" +
                JsonEscape(command) + L",\"args\":" + JsonEscape(args) + L"}}";
    }
    SubmitResults(q, json + L"]", TRUE);
}

static std::wstring StringSetting(const wchar_t* fmt, int i) {
    PCWSTR s = Wh_GetStringSetting(fmt, i);
    std::wstring r = s ? s : L"";
    Wh_FreeStringSetting(s);
    return r;
}
static void LoadCommands() {
    std::vector<CustomCommand> cmds;
    for (int i = 0; i < 200; i++) {
        CustomCommand c{Trim(StringSetting(L"commands[%d].keyword", i)), StringSetting(L"commands[%d].title", i),
                        Trim(StringSetting(L"commands[%d].command", i)), StringSetting(L"commands[%d].args", i)};
        if (c.keyword.empty() && c.command.empty()) break;
        if (!c.keyword.empty() && !c.command.empty()) cmds.push_back(std::move(c));
    }
    std::lock_guard l(g_commandsLock);
    g_commands.swap(cmds);
}

struct BuiltIn {
    const wchar_t* json;
    void (*query)(UINT64 q, std::wstring const& text);
    void (*invoke)(std::wstring const& item);
    std::atomic<UINT64> handle{0};
};
static BuiltIn g_calc{L"{\"id\":\"calc\",\"name\":\"Calculator\",\"description\":\"Sums and unit conversions\",\"glyph\":\"\\uE8EF\"}",
                      CalcQuery, nullptr};
static BuiltIn g_kill{L"{\"id\":\"kill\",\"name\":\"Kill process\",\"description\":\"kill <name> ends a process\",\"glyph\":\"\\uE71A\"}",
                      KillQuery, KillInvoke};
static BuiltIn g_custom{L"{\"id\":\"commands\",\"name\":\"Custom commands\",\"description\":\"Keywords from the mod settings\",\"glyph\":\"\\uE756\"}",
                        CommandsQuery, nullptr};

static void SyncBuiltIn(BuiltIn& b, bool want) {
    if (want && !b.handle) {
        ProviderCallbacks cb;
        cb.onQuery = [&b](UINT64 q, std::wstring const& text, std::wstring const&) { b.query(q, text); };
        if (b.invoke)
            cb.onInvoke = [&b](std::wstring const& item, std::wstring const&, std::wstring const&, std::wstring const&) {
                b.invoke(item);
            };
        std::wstring err;
        UINT64 h = RegisterProvider(b.json, std::move(cb), &err);
        if (!h) Wh_Log(L"built-in provider failed: %s", err.c_str());
        b.handle = h;
    } else if (!want) {
        if (UINT64 h = b.handle.exchange(0)) UnregisterProvider(h);
    }
}
