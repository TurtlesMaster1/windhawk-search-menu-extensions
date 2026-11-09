# Install out\mod.dll as the local "search-menu-extensions" Windhawk mod, the way the editor does,
# without opening the editor. Needs admin (writes ProgramData\Windhawk and HKLM). Logging is turned
# on so test\dbwin.py can capture the mod's log. The engine hot-swaps the mod when
# SettingsChangeTime changes.
param([string]$version = '0.1.0')
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
Set-ItemProperty $key SettingsChangeTime ([int][double]::Parse((Get-Date -UFormat %s))) -Type DWord
"installed $name"
