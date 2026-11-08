# Restart the composer and inject the given test DLLs as soon as it relaunches.
param([string[]]$dlls)
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$old = Get-CimInstance Win32_Process -Filter "Name='shellhost.exe'" | ? { $_.CommandLine -match 'SystemComposerControl' }
$old | % { Stop-Process -Id $_.ProcessId -Force }
Start-Sleep -Milliseconds 150
$before = (Get-Process shellhost -EA 0).Id
$A=[System.Windows.Automation.AutomationElement]; $TS=[System.Windows.Automation.TreeScope]
$tray=$A::RootElement.FindFirst($TS::Children,(New-Object System.Windows.Automation.PropertyCondition($A::ClassNameProperty,'Shell_TrayWnd')))
$btn=$tray.FindFirst($TS::Descendants,(New-Object System.Windows.Automation.PropertyCondition($A::AutomationIdProperty,'ComposerControlTaskbarUx')))
$o=$null; if($btn.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern,[ref]$o)){ $o.Invoke() } else { $btn.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Toggle() }
$sw=[Diagnostics.Stopwatch]::StartNew(); $new=$null
while ($sw.ElapsedMilliseconds -lt 10000 -and -not $new) { $new = Get-Process shellhost -EA 0 | ? { $before -notcontains $_.Id } | select -First 1 }
if (-not $new) { 'no new composer'; exit 1 }
"composer $($new.Id) after $($sw.ElapsedMilliseconds)ms"
Start-Sleep -Milliseconds 60
foreach ($d in $dlls) {
  for ($i = 0; $i -lt 3; $i++) {
    $r = & (Join-Path $PSScriptRoot '..\out\inject.exe') $new.Id $d init
    $r
    if ($r -match 'done') { break }
    Start-Sleep -Milliseconds 15
  }
}
