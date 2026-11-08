param([string]$text = 'wh t', [string]$item = 'Demo toggle', [string]$how = 'auto')
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Windows.Forms
$sig='[DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e); [DllImport("user32.dll")] public static extern IntPtr FindWindow(string c, string n); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);'
$K=Add-Type -MemberDefinition $sig -Name K4 -Namespace W4 -PassThru
$K::keybd_event(0x12,0,0,[UIntPtr]::Zero); $K::keybd_event(0x12,0,2,[UIntPtr]::Zero); [void]$K::SetForegroundWindow($K::FindWindow('Shell_TrayWnd',$null)); Start-Sleep -Milliseconds 300; $K::keybd_event(0x5B,0,0,[UIntPtr]::Zero); $K::keybd_event(0x53,0,0,[UIntPtr]::Zero); $K::keybd_event(0x53,0,2,[UIntPtr]::Zero); $K::keybd_event(0x5B,0,2,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 1200
[System.Windows.Forms.SendKeys]::SendWait('^a'); [System.Windows.Forms.SendKeys]::SendWait($text)
Start-Sleep -Milliseconds 2000
$focus=[System.Windows.Automation.AutomationElement]::FocusedElement
$root=[System.Windows.Automation.AutomationElement]::RootElement
$walker=[System.Windows.Automation.TreeWalker]::ControlViewWalker
$top=$root.FindFirst([System.Windows.Automation.TreeScope]::Children,(New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty,'ShellUXFrameWindowClass'))); if(-not $top){ 'composer window not found'; exit }
$all=$top.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
$target=$null
foreach($e in $all){ if($e.Current.Name -like "$item*" -and $e.Current.ControlType -eq [System.Windows.Automation.ControlType]::ListItem){ $target=$e; break } }
if(-not $target){ "item not found; focus=$($focus.Current.Name) pid=$($focus.Current.ProcessId) top=$($top.Current.ClassName)"; foreach($e in $all){ if($e.Current.Name){ "  " + $e.Current.Name } }; exit }
"found: $($target.Current.Name)"
"patterns: " + (($target.GetSupportedPatterns() | % { $_.ProgrammaticName }) -join ', ')
$kids=$target.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
foreach($k in $kids){ "  child {0} '{1}' [{2}] {3}" -f $k.Current.ControlType.ProgrammaticName,$k.Current.Name,$k.Current.AutomationId,(($k.GetSupportedPatterns()|%{$_.ProgrammaticName}) -join ',') }
$done=$false
foreach($k in @($target)+@($kids)){
  if($done){break}
  $o=$null
  if(($how -eq 'auto' -or $how -eq 'toggle') -and $k.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern,[ref]$o)){ $o.Toggle(); "toggled via $($k.Current.ControlType.ProgrammaticName)"; $done=$true; continue }
  if(($how -eq 'auto' -or $how -eq 'invoke') -and $k.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern,[ref]$o)){ $o.Invoke(); "invoked via $($k.Current.ControlType.ProgrammaticName) '$($k.Current.Name)'"; $done=$true; continue }
}
Start-Sleep -Milliseconds 1500
$all=$top.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
foreach($e in $all){ $n=$e.Current.Name; if($n){ "  after: {0} | {1}" -f $e.Current.ControlType.ProgrammaticName.Replace('ControlType.',''),$n } }
$K::keybd_event(0x1B,0,0,[UIntPtr]::Zero); $K::keybd_event(0x1B,0,2,[UIntPtr]::Zero)
