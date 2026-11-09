// ==WindhawkMod==
// @id              search-menu-extensions
// @name            New Windows Search Extensions (library)
// @description     For the NEW Windows 11 Insider search box (composer search, build 26320+), not classic Start/taskbar search. Lets other mods, scripts and programs add results, actions, toggles and cards, mixed in with the native results
// @version         0.1.0
// @author          TurtlesMaster1
// @github          https://github.com/TurtlesMaster1
// @include         shellhost.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lshlwapi -lshell32 -luuid -ladvapi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
> **Updated for Build 26320.9876 (Windows 11 Insider Beta).** Insider builds
> change quickly, so newer builds may break or move this.

# New Windows Search Extensions (library)

**This is only for the NEW Windows search**: the compact "composer" search box
on recent Windows 11 Insider builds (hosted by
`shellhost.exe loadComponent SystemComposerControl`, the one with inline
actions like "Turn on dark mode" and the volume slider). It does **nothing** for
the classic Start menu search, the old taskbar search flyout (`SearchHost.exe`)
or Windows 10.

![Demo provider items mixed into the new Windows search](https://raw.githubusercontent.com/TurtlesMaster1/windhawk-search-menu-extensions/main/images/demo.png)

A **dependency mod**: on its own it only adds an optional demo provider (type
`wh hello` to see an echo/copy item, a toggle, a "run" item and a card). Other mods (or any program/script) use it to put items into the new
search box. Providers see every query and decide for themselves when to answer,
so no prefix is needed.

![A pipe client showing live weather with an Adaptive Card](https://raw.githubusercontent.com/TurtlesMaster1/windhawk-search-menu-extensions/main/images/weather.png)

*Above: `examples/weather_client.py`, an out-of-process provider talking over
the named pipe.*

Items are injected as genuine native search providers. Windows' own search is
built from *Palette extensions* (`Windows.UI.Palettes`): Apps, Files, Settings,
Windows Actions, Command Line and so on are each one extension. This mod adds
one more extension per registered provider, so your items are real
`PaletteItem`s. They mix into the native list with native ranking, keyboard
navigation, icons, toggles and Adaptive Card content.

## Two ways to provide items

### 1. From another Windhawk mod (in-process)

Your mod must also `@include shellhost.exe`. Find this library's DLL and call its
exported C API (Windhawk exports every symbol of a mod DLL):

```cpp
typedef struct WSE_ProviderCallbacks {
    UINT32 cbSize;
    void* ctx;
    // Called for every query (text may be empty = zero state). Answer with
    // WSE_SubmitResults(queryHandle, itemsJson, done) from any thread, now or later.
    void (WINAPI* onQuery)(void* ctx, UINT64 queryHandle, PCWSTR text, PCWSTR queryJson);
    // An item was invoked: actionId is "invoke", "toggle" or a card action id.
    void (WINAPI* onInvoke)(void* ctx, PCWSTR itemId, PCWSTR actionId, PCWSTR dataJson, PCWSTR inputsJson);
    // Optional. Query abandoned (user typed more / closed search).
    void (WINAPI* onCancel)(void* ctx, UINT64 queryHandle);
    // Optional. The library is unloading: drop the provider handle, don't call the API again.
    void (WINAPI* onDetach)(void* ctx);
} WSE_ProviderCallbacks;

int    WINAPI WSE_GetApiVersion(void);                                   // 1
UINT64 WINAPI WSE_RegisterProvider(PCWSTR providerJson, const WSE_ProviderCallbacks* cb);
BOOL   WINAPI WSE_SubmitResults(UINT64 queryHandle, PCWSTR itemsJson, BOOL done);
BOOL   WINAPI WSE_SetStaticItems(UINT64 provider, PCWSTR itemsJson);     // items always offered (GetItems)
BOOL   WINAPI WSE_UpdateCard(UINT64 provider, PCWSTR itemId, PCWSTR dataJson); // live-update a card's data
BOOL   WINAPI WSE_NotifyChanged(UINT64 provider);                        // ask search to re-query
BOOL   WINAPI WSE_UnregisterProvider(UINT64 provider);                   // waits for running callbacks
```

Finding the library: enumerate the modules of the process and use the first one
that exports `WSE_GetApiVersion`. Mods load in any order, so retry for a few
seconds if it isn't there yet (and again later: the library can be disabled,
updated or re-enabled while your mod is running; `onDetach` tells you it's going).

Callbacks run on the library's worker threads, never on Windows' own threads.
Call `WSE_UnregisterProvider` in your `Wh_ModUninit`: it returns once none of
your callbacks is running any more, so your DLL can unload safely. (Calling it
from inside one of your own callbacks is fine too; it then waits for the others.)

### 2. From any process (named pipe)

Connect to `\\.\pipe\WindhawkSearchExtensions-<session id>` (the Windows
session id of your process, e.g. `ProcessIdToSessionId`) and exchange
**newline-delimited UTF-8 JSON** messages. Each connection can register any
number of providers. They're removed automatically when the connection closes.

Only processes running as the same user can open the pipe. Any of them can
register a provider, and a provider sees the text typed into search. Turn off
**Named pipe API** if you don't use it.

Client to library:

```json
{"op":"register","provider":{"id":"weather","name":"Weather","glyph":"\uE706"}}
{"op":"results","queryId":"17","items":[ ... ],"done":true}
{"op":"setItems","provider":"weather","items":[ ... ]}
{"op":"updateCard","provider":"weather","itemId":"now","data":{ ... }}
{"op":"refresh","provider":"weather"}
{"op":"unregister","provider":"weather"}
```

Library to client:

```json
{"op":"registered","provider":"weather","extensionId":"Windhawk.weather"}
{"op":"query","provider":"weather","queryId":"17","text":"weather lond","query":{...}}
{"op":"cancel","provider":"weather","queryId":"17"}
{"op":"invoke","provider":"weather","itemId":"now","actionId":"invoke","data":"","inputs":""}
{"op":"error","message":"..."}
```

## Provider JSON

```json
{
  "id": "weather",                       // unique, [A-Za-z0-9._-]
  "name": "Weather",                     // shown in search settings/diagnostics
  "description": "Current conditions",
  "glyph": "\uE706", "font": "Segoe Fluent Icons",  // or "icon": "C:\\path\\icon.png"
  "zeroState": false,                    // also asked when the box is empty
  "kinds": ["QueryResult.SystemComposer"],          // advanced: palette kinds this source serves
  "itemKinds": ["Command"]               // default kinds for items
}
```

## Item JSON

```json
{
  "id": "now",                           // stable id within the provider
  "title": "London: 14°C, light rain",
  "subtitle": "Feels like 12°C",         // string or array of strings
  "icon": {"glyph": "\uE9E2", "font": "Segoe Fluent Icons"},  // or {"uri": "https://..."} / {"path": "C:\\x.png"} / "C:\\x.png"
  "tags": [{"text": "Live", "tooltip": "Updated just now"}],
  "kinds": ["Command"],
  "score": 0.9,                          // goes to the native ranking (KnownPaletteProperties.Score)
  "properties": {"Path": "C:\\Users"},   // any extra KnownPaletteProperties
  "action": {"type": "callback"},        // default: onInvoke(id, "invoke")
  //  {"type":"uri","uri":"https://bbc.co.uk/weather"}
  //  {"type":"run","command":"notepad.exe","args":"x.txt"}
  //  {"type":"copy","text":"14°C"}
  //  {"type":"toggle","isToggled":true}   -> onInvoke(id, "toggle", "true"|"false")
  "card": {                              // optional Adaptive Card content (template + data)
    "template": { "type": "AdaptiveCard", "version": "1.5", "body": [ ... ], "actions": [ ... ] },
    "data": { ... }                      // Action.Submit/Execute -> onInvoke(id, actionId, data, inputs)
  }
}
```

## Notes

* This mod only works on builds that have the new composer search
  (`Windows.UI.Palettes`). Elsewhere it does nothing.
* Windows reads its provider list once, when the composer starts. If the mod
  is enabled while the composer is already running, it restarts the composer
  (setting **Restart search when enabled late**). The composer comes back the
  next time you open search. Providers registered later (other mods, pipe
  clients) are picked up live.
* When the mod is disabled or updated, Windows may still hold some of its
  objects (items it's showing, finished queries). The library then restarts the
  composer instead of unloading under them; search reopens normally next time.
* **Card buttons:** cards render under the selected row and you can update them
  live (`WSE_UpdateCard` / `updateCard`). But Windows ignores Action.Submit
  buttons in that inline preview ("slot previews do not support card actions").
  Put the action on the item itself.
* **Pipe clients:** if you open the pipe as an ordinary synchronous handle
  (Python `open()`, .NET `FileStream` without async), don't write from one
  thread while another is blocked reading. Windows serializes I/O on
  synchronous handles and the write waits forever. Either answer from the
  reading thread or open the pipe overlapped.
* Enable logging for the mod in Windhawk to see the queries and what Windows
  asks of the providers.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- demoProvider: true
  $name: Demo provider
  $description: Adds sample items when you type "wh" (echo, toggle, card, run). Handy to check the library works.
- pipeServer: true
  $name: Named pipe API
  $description: Let other processes and scripts (same user only) add items through \\.\pipe\WindhawkSearchExtensions-<session id>
- queryTimeoutMs: 2500
  $name: Provider timeout (ms)
  $description: How long to wait for a provider's results before showing what it has so far
- restartIfLate: true
  $name: Restart search when enabled late
  $description: If the mod is enabled while the search composer is already running, restart it (it reopens on next use) so Windows picks up the providers
*/
// ==/WindhawkModSettings==
