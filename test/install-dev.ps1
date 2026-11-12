# Install out\mod.dll as the local "search-menu-extensions" Windhawk mod, the way the editor does,
# without opening the editor. Needs admin (writes ProgramData\Windhawk and HKLM). Logging is turned
# on so test\dbwin.py can capture the mod's log. The engine hot-swaps the mod when
# SettingsChangeTime changes. Settings are written too: a value that is already there is kept,
# unless -Defaults is given, which resets the known settings to the defaults in src/head.cpp.
param([string]$version = '0.1.0', [switch]$Defaults)
$ErrorActionPreference = 'Stop'
$id = 'local@search-menu-extensions'
$mods = 'C:\ProgramData\Windhawk\Engine\Mods\64'
$key = "HKLM:\SOFTWARE\Windhawk\Engine\Mods\$id"
$src = Join-Path $PSScriptRoot '..\out\mod.dll'
$name = "{0}_{1}_{2}.dll" -f $id, $version, (Get-Random -Minimum 100000 -Maximum 999999)
Copy-Item $src (Join-Path $mods $name)
if (-not (Test-Path $key)) { New-Item $key -Force | Out-Null }
Set-ItemProperty $key LibraryFileName $name
Set-ItemProperty $key Version $version
Set-ItemProperty $key Include 'shellhost.exe'
Set-ItemProperty $key LoggingEnabled 1 -Type DWord
Set-ItemProperty $key Disabled 0 -Type DWord
Remove-ItemProperty "$key\Settings" debugLog -ErrorAction SilentlyContinue
# The engine reads a missing setting as off, and the editor would fill these in from head.cpp.
# Keep this list in sync with WindhawkModSettings in src/head.cpp.
if (-not (Test-Path "$key\Settings")) { New-Item "$key\Settings" -Force | Out-Null }
$defaults = [ordered]@{
    calculator = 1; killProcess = 1; demoProvider = 0; pipeServer = 0; queryTimeoutMs = 2500; restartIfLate = 1
    'commands[0].keyword' = 'g'; 'commands[0].title' = 'Search Google for {query}'
    'commands[0].command' = 'https://www.google.com/search?q={query}'; 'commands[0].args' = ''
    'commands[1].keyword' = 'yt'; 'commands[1].title' = 'Search YouTube for {query}'
    'commands[1].command' = 'https://www.youtube.com/results?search_query={query}'; 'commands[1].args' = ''
}
foreach ($setting in $defaults.GetEnumerator()) {
    $present = Get-ItemProperty "$key\Settings" -Name $setting.Key -ErrorAction SilentlyContinue
    if ($Defaults -or -not $present) {
        if ($setting.Value -is [int]) { Set-ItemProperty "$key\Settings" $setting.Key $setting.Value -Type DWord }
        else { Set-ItemProperty "$key\Settings" $setting.Key $setting.Value -Type String }
    }
}
Set-ItemProperty $key SettingsChangeTime ([int][double]::Parse((Get-Date -UFormat %s))) -Type DWord
"installed $name"
