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
    if (v == std::floor(v) && std::fabs(v) <= 9007199254740992.0)  // whole numbers up to 2^53 in full
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
    bool pct = false;      // the operand just parsed was a postfix "N%"
    bool termPct = false;  // the term just parsed was a single "N%"

    void Ws() { while (*p == L' ') p++; }
    static bool StartsOperand(wchar_t c) { return iswalnum(c) || c == L'(' || c == L'.'; }
    // A "%" that isn't followed by another operand is a postfix percent ("10 % 3" is a remainder).
    bool PostfixPercent() {
        const wchar_t* s = p;
        while (*s == L' ') s++;
        if (*s != L'%') return false;
        const wchar_t* after = s + 1;
        while (*after == L' ') after++;
        if (StartsOperand(*after)) return false;
        p = s + 1;
        ops++;
        return true;
    }
    // "50 + 10%" is 55: a percentage on the right takes that share of the left side, like a pocket
    // calculator, unless the left side is a percentage too ("20% + 5%" is 0.25, not 0.21).
    double Expr() {
        double v = Term();
        bool lhsPct = termPct;
        double mag = 0;
        for (;;) {
            Ws();
            bool minus = *p == L'-' || *p == 0x2212;
            if (*p != L'+' && !minus) return v;
            p++; ops++;
            double t = Term();
            bool tp = termPct;
            if (tp && !lhsPct) t *= v;
            double nv = minus ? v - t : v + t;
            // A sum that cancels ("45.30-20.10-25.20") leaves binary noise: snap it to 0. The threshold
            // is relative to the largest operand so far, so a real small result isn't touched.
            mag = std::max({mag, std::fabs(v), std::fabs(t)});
            if (std::fabs(nv) < mag * 1e-13 && std::fabs(nv) < std::max(std::fabs(v), std::fabs(t)) * 1e-3) nv = 0;
            v = nv;
            lhsPct = lhsPct && tp;
        }
    }
    double Term() {
        double v = Unary();
        termPct = pct;
        for (;;) {
            Ws();
            wchar_t c = *p;
            if (c == L'*' || c == 0xD7) { p++; ops++; v *= Unary(); termPct = false; }
            else if (c == L'/' || c == 0xF7) { p++; ops++; v /= Unary(); termPct = false; }
            else if (c == L'%') { p++; ops++; v = std::fmod(v, Unary()); termPct = false; }  // "10 % 3"
            else return v;
        }
    }
    double Unary() {
        Ws();
        if (++depth > 64) { ok = false; return 0; }
        double v;
        bool isPct = false;
        if (*p == L'-' || *p == 0x2212) { p++; v = -Unary(); isPct = pct; }
        else if (*p == L'+') { p++; v = Unary(); isPct = pct; }
        else {
            v = Primary();
            if (PostfixPercent()) { v /= 100; isPct = true; }  // binds to this operand, before ^ * /
            Ws();
            if (*p == L'^') { p++; ops++; v = std::pow(v, Unary()); isPct = false; }
        }
        depth--;
        pct = isPct;
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
    {L"km kilometer kilometers kilometre kilometres kms", 'L', 1000},
    {L"in inch inches \"", 'L', 0.0254},
    {L"ft foot feet '", 'L', 0.3048},
    {L"yd yard yards yds", 'L', 0.9144},
    {L"mi mile miles", 'L', 1609.344},
    {L"nmi", 'L', 1852},
    {L"mg milligram milligrams", 'M', 1e-6},
    {L"g gram grams", 'M', 0.001},
    {L"kg kilogram kilograms kilo kilos kgs", 'M', 1},
    {L"t tonne tonnes", 'M', 1000},
    {L"oz ounce ounces", 'M', 0.028349523125},
    {L"lb lbs pound pounds", 'M', 0.45359237},
    {L"st stone", 'M', 6.35029318},
    {L"ml milliliter milliliters millilitre millilitres", 'V', 0.001},
    {L"l liter liters litre litres", 'V', 1},
    {L"tsp teaspoon teaspoons", 'V', 0.00492892159375},
    {L"tbsp tablespoon tablespoons", 'V', 0.01478676478125},
    {L"floz", 'V', 0.0295735295625},
    {L"cup cups", 'V', 0.2365882365},
    {L"pt pint pints", 'V', 0.473176473},
    {L"qt quart quarts", 'V', 0.946352946},
    {L"gal gallon gallons", 'V', 3.785411784},
    {L"ms millisecond milliseconds", 'D', 0.001},
    {L"s sec secs second seconds", 'D', 1},
    {L"min mins minute minutes", 'D', 60},
    {L"h hr hrs hour hours", 'D', 3600},
    {L"d day days", 'D', 86400},
    {L"wk week weeks wks", 'D', 604800},
    {L"yr year years yrs", 'D', 31557600},
    {L"m/s", 'S', 1},
    {L"km/h kmh kph km/hr", 'S', 1 / 3.6},
    {L"mph", 'S', 0.44704},
    {L"kn knot knots", 'S', 1852.0 / 3600},
    {L"ft/s", 'S', 0.3048},
    // Bytes are binary, as File Explorer shows them (1 MB = 1048576 bytes). Bits are decimal (1 Mb =
    // 1000000 bits). The byte rows come first, so "mb" still means megabytes while "Mb" is megabits.
    {L"B byte bytes", 'B', 1},
    {L"KB kilobyte kilobytes", 'B', 1024.0},
    {L"MB megabyte megabytes", 'B', 1048576.0},
    {L"GB gigabyte gigabytes", 'B', 1073741824.0},
    {L"TB terabyte terabytes", 'B', 1099511627776.0},
    {L"bit b bits", 'B', 0.125},
    {L"Kb kbit kbits kilobit kilobits", 'B', 125.0},
    {L"Mb mbit mbits megabit megabits", 'B', 125000.0},
    {L"Gb gbit gbits gigabit gigabits", 'B', 125000000.0},
    {L"Tb tbit tbits terabit terabits", 'B', 125000000000.0},
    {L"\u00b0C c celsius \u2103", 'T', 1},
    {L"\u00b0F f fahrenheit \u2109", 'T', 2},
    {L"K kelvin", 'T', 3},
};

// Exact case first, so "Mb" is megabits and "MB" megabytes; then any case, so "mb" or "KG" still work.
static const Unit* FindUnitAs(std::wstring const& name, std::wstring* shown, bool exactCase) {
    for (auto& u : kUnits) {
        const wchar_t* s = u.names;
        while (*s) {
            const wchar_t* e = wcschr(s, L' ');
            size_t n = e ? (size_t)(e - s) : wcslen(s);
            bool same = n == name.size() && (exactCase ? !wcsncmp(s, name.c_str(), n) : !_wcsnicmp(s, name.c_str(), n));
            if (same) {
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
static const Unit* FindUnit(std::wstring const& name, std::wstring* shown) {
    const Unit* u = FindUnitAs(name, shown, true);
    return u ? u : FindUnitAs(name, shown, false);
}

// Replaces every case-insensitive occurrence of from (lowercase) with to.
static void ReplaceCI(std::wstring& s, const wchar_t* from, const wchar_t* to) {
    size_t n = wcslen(from), m = wcslen(to);
    for (size_t i = Lower(s).find(from); i != std::wstring::npos; i = Lower(s).find(from, i + m)) s.replace(i, n, to);
}

// "<number or expression> <unit> to|in <unit>"
static bool ConvertUnits(std::wstring const& input, std::wstring* title, std::wstring* value) {
    std::wstring text = input;
    ReplaceCI(text, L"fl. oz", L"floz");  // "fl oz" is one unit
    ReplaceCI(text, L"fl oz", L"floz");
    std::wstring t = Lower(text);  // same length as text, so a position in t is a position in text
    size_t a = t.rfind(L" to "), b = t.rfind(L" in ");
    size_t pos = a == std::wstring::npos ? b : (b == std::wstring::npos ? a : std::max(a, b));
    if (pos == std::wstring::npos) return false;
    std::wstring left = Trim(text.substr(0, pos)), toName = Trim(text.substr(pos + 4));  // unit names keep their case
    size_t i = left.size();
    while (i > 0 && !iswdigit(left[i - 1]) && left[i - 1] != L')' && left[i - 1] != L'.' && left[i - 1] != L' ') i--;
    std::wstring fromName = left.substr(i), num = Trim(left.substr(0, i));
    if (fromName.empty() || toName.empty() || num.empty()) return false;
    std::wstring fromShown, toShown;
    const Unit* from = FindUnit(fromName, &fromShown);
    const Unit* to = FindUnit(toName, &toShown);
    double x;
    if (!from || !to || from->cat != to->cat || !Evaluate(num, false, &x)) return false;
    if (fromShown == L"floz") fromShown = L"fl oz";
    if (toShown == L"floz") toShown = L"fl oz";
    double y;
    if (from->cat == 'T') {
        double k = from->f == 1 ? x + 273.15 : from->f == 2 ? (x - 32) * 5 / 9 + 273.15 : x;
        y = to->f == 1 ? k - 273.15 : to->f == 2 ? (k - 273.15) * 9 / 5 + 32 : k;
        if (std::fabs(y) < 1e-9) y = 0;
    } else {
        y = x * from->f / to->f;
    }
    if (!std::isfinite(y)) return false;
    *value = Num(y, 12);
    *title = Num(x, 12) + L" " + fromShown + L" = " + *value + L" " + toShown;
    return true;
}

// Digit groups joined by - or /, like "2026-11-11", "11/10/2026", "555-123-4567" or "0800-123-456":
// dates, phone numbers and IDs, not sums. Two or more separators, or a group with a leading zero.
static bool IsDateLike(std::wstring const& t) {
    size_t start = !t.empty() && t[0] == L'+' ? 1 : 0, seps = 0;
    for (size_t k = start; k <= t.size(); k++) {
        if (k == t.size() || t[k] == L'-' || t[k] == L'/') {
            size_t len = k - start;
            if (len == 0) return false;
            if (len > 1 && t[start] == L'0') return true;
            if (k < t.size()) seps++;
            start = k + 1;
        } else if (t[k] < L'0' || t[k] > L'9') {
            return false;
        }
    }
    return seps >= 2;
}

static void CalcQuery(UINT64 q, std::wstring const& text) {
    std::wstring t = Trim(text), title, value;
    bool forced = !t.empty() && t[0] == L'=';  // "=2026-11-11" is always a sum
    if (forced) t = Trim(t.substr(1));
    if (!t.empty() && t.back() == L'=') t = Trim(t.substr(0, t.size() - 1));  // "1+1="
    double v;
    if (ConvertUnits(t, &title, &value)) {
    } else if ((forced || !IsDateLike(t)) && Evaluate(t, true, &v)) {
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

static ULONGLONG ProcessCreated(HANDLE h) {
    FILETIME c{}, e, k, u;
    return GetProcessTimes(h, &c, &e, &k, &u) ? ((ULONGLONG)c.dwHighDateTime << 32) | c.dwLowDateTime : 0;
}

// "kill <part of a process name>": Enter ends that process. Nothing is listed until two letters are
// typed, so "kill" + Enter can't end something by accident, and "kill e" doesn't list half the machine.
static void KillQuery(UINT64 q, std::wstring const& text) {
    std::wstring t = Lower(Trim(text));
    std::wstring filter = t.rfind(L"kill ", 0) == 0 ? Trim(t.substr(5)) : L"";
    if (filter.size() > 4 && filter.compare(filter.size() - 4, 4, L".exe") == 0) filter.resize(filter.size() - 4);
    if (filter.size() < 2) {
        SubmitResults(q, L"[]", TRUE);
        return;
    }
    struct Proc { DWORD pid; ULONGLONG created; std::wstring name, path; SIZE_T mem; int tier; };
    std::vector<Proc> procs;
    DWORD self = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{sizeof(pe)};
        for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
            if (pe.th32ProcessID <= 4 || pe.th32ProcessID == self) continue;  // never the search host itself
            // Match the name without ".exe", so "exe" or "." doesn't match every process.
            std::wstring base = Lower(pe.szExeFile);
            if (size_t dot = base.rfind(L'.'); dot != std::wstring::npos && dot > 0) base.resize(dot);
            size_t at = base.find(filter);
            if (at == std::wstring::npos) continue;
            // The exact name first, then names that start with it, then matches after a separator.
            int tier = base.size() == filter.size() ? 0 : at == 0 ? 1 : (wcschr(L"-_. ", base[at - 1]) ? 2 : 3);
            // Only list what we could actually end.
            HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!h) continue;
            Proc p{pe.th32ProcessID, ProcessCreated(h), pe.szExeFile, L"", 0, tier};
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
    std::stable_sort(procs.begin(), procs.end(), [](Proc const& a, Proc const& b) {
        return a.tier != b.tier ? a.tier < b.tier : a.mem > b.mem;  // the best name match first, then the biggest
    });
    if (procs.size() > 8) procs.resize(8);
    std::wstring json = L"[";
    double score = 0.9;  // in that order
    for (auto& p : procs) {
        wchar_t sub[128];
        swprintf(sub, 128, L"PID %lu \u00b7 %.0f MB \u00b7 Enter to end it", p.pid, p.mem / 1048576.0);
        if (json.size() > 1) json += L",";
        json += L"{\"id\":\"" + std::to_wstring(p.pid) + L":" + std::to_wstring(p.created) + L"\",\"title\":" + JsonEscape(L"End " + p.name) +
                L",\"subtitle\":" + JsonEscape(sub) + L",\"icon\":" +
                (p.path.empty() ? std::wstring(L"{\"glyph\":\"\\uE71A\"}") : L"{\"path\":" + JsonEscape(p.path) + L"}") +
                L",\"score\":" + Num(score, 3) + L"}";
        score -= 0.01;
    }
    SubmitResults(q, json + L"]", TRUE);
}
// The id is "<pid>:<creation time>": PIDs are reused, so make sure it's still the listed process.
static void KillInvoke(std::wstring const& item) {
    wchar_t* end = nullptr;
    DWORD pid = wcstoul(item.c_str(), &end, 10);
    ULONGLONG created = end && *end == L':' ? wcstoull(end + 1, nullptr, 10) : 0;
    if (pid <= 4 || pid == GetCurrentProcessId() || !created) return;
    HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return;
    bool same = ProcessCreated(h) == created;
    BOOL ok = same && TerminateProcess(h, 1);
    CloseHandle(h);
    Wh_Log(L"kill %lu: %s", pid, ok ? L"ok" : same ? L"failed" : L"process changed");
}

// Custom commands from the settings: keyword -> program, file or URL. "{query}" stands for
// whatever is typed after the keyword.
struct CustomCommand { std::wstring keyword, title, command, args; };
static std::mutex g_commandsLock;
static std::vector<CustomCommand> g_commands;  // plain strings

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
// {query} in the arguments goes in as typed, so several words are several arguments, as on a command
// line (wrap it in quotes, "{query}", to keep them together). Inside a URL it is percent-encoded.
static std::wstring FillArgs(std::wstring s, std::wstring const& query) {
    for (size_t i = 0; (i = s.find(L"{query}", i)) != std::wstring::npos;) {
        std::wstring with = s.substr(0, i).find(L"://") != std::wstring::npos ? UrlEncode(query) : query;
        s.replace(i, 7, with);
        i += with.size();
    }
    return s;
}

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
            // "g cats": the keyword, then the text to pass on. The bare keyword shows nothing.
            if (lt.rfind(kw + L" ", 0) != 0) continue;
            rest = Trim(t.substr(kw.size()));
            if (rest.empty()) continue;
        } else {
            // No {query}: the keyword, or the start of it, or part of the title (its placeholder doesn't count).
            std::wstring plainTitle = Lower(FillQuery(c.title, L""));
            if (lt != kw && (lt.size() < 2 || (kw.rfind(lt, 0) != 0 && plainTitle.find(lt) == std::wstring::npos))) continue;
        }
        bool url = c.command.find(L"://") != std::wstring::npos;
        std::wstring command = FillQuery(c.command, url ? UrlEncode(rest) : rest), args = FillArgs(c.args, rest);
        std::wstring title = Trim(FillQuery(c.title.empty() ? c.command : c.title, rest));
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
    std::wstring r = s;
    Wh_FreeStringSetting(s);
    return r;
}
static void LoadCommands() {
    std::vector<CustomCommand> cmds;
    int blanks = 0;  // a cleared entry keeps its place in the settings list: skip it, and a long run ends the list
    for (int i = 0; i < 200 && blanks < 16; i++) {
        CustomCommand c{Trim(StringSetting(L"commands[%d].keyword", i)), StringSetting(L"commands[%d].title", i),
                        Trim(StringSetting(L"commands[%d].command", i)), StringSetting(L"commands[%d].args", i)};
        if (c.keyword.empty() && c.command.empty()) {
            blanks++;
            continue;
        }
        blanks = 0;
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
