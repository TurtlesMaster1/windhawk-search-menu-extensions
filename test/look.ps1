param([string]$text = 'wh hello', [int]$wait = 2500, [string]$keys = '')
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Windows.Forms,System.Drawing
$sig='[DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e); [DllImport("user32.dll")] public static extern IntPtr FindWindow(string c, string n); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);'
$K=Add-Type -MemberDefinition $sig -Name K3 -Namespace W3 -PassThru
$K::keybd_event(0x12,0,0,[UIntPtr]::Zero); $K::keybd_event(0x12,0,2,[UIntPtr]::Zero); [void]$K::SetForegroundWindow($K::FindWindow('Shell_TrayWnd',$null)); Start-Sleep -Milliseconds 300; $K::keybd_event(0x5B,0,0,[UIntPtr]::Zero); $K::keybd_event(0x53,0,0,[UIntPtr]::Zero); $K::keybd_event(0x53,0,2,[UIntPtr]::Zero); $K::keybd_event(0x5B,0,2,[UIntPtr]::Zero)
Start-Sleep -Milliseconds 1200
[System.Windows.Forms.SendKeys]::SendWait($text)
Start-Sleep -Milliseconds $wait
try {
  $b=[System.Windows.Forms.Screen]::PrimaryScreen.Bounds; $bmp=New-Object System.Drawing.Bitmap $b.Width,$b.Height
  $g=[System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location,[System.Drawing.Point]::Empty,$b.Size)
  $bmp.Save("$env:TEMP\wse-look.png"); "shot saved"
} catch { "shot failed: $_" }
$root=[System.Windows.Automation.AutomationElement]::RootElement
$focus=[System.Windows.Automation.AutomationElement]::FocusedElement
"FOCUS: $($focus.Current.Name) pid=$($focus.Current.ProcessId) [$($focus.Current.ClassName)]"
$pidC=$focus.Current.ProcessId
$walker=[System.Windows.Automation.TreeWalker]::ControlViewWalker
$top=$root.FindFirst([System.Windows.Automation.TreeScope]::Children,(New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty,'ShellUXFrameWindowClass'))); if(-not $top){ 'composer window not found'; exit }
"TOP: $($top.Current.Name) [$($top.Current.ClassName)] pid=$($top.Current.ProcessId)"
$all=$top.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
foreach($e in $all){ $n=$e.Current.Name; if($n){ "  {0} | {1} | {2}" -f $e.Current.ControlType.ProgrammaticName.Replace('ControlType.',''),$n,$e.Current.AutomationId } }
if ($keys) { [System.Windows.Forms.SendKeys]::SendWait($keys); Start-Sleep -Milliseconds 1500; "sent keys $keys" } else { $K::keybd_event(0x1B,0,0,[UIntPtr]::Zero); $K::keybd_event(0x1B,0,2,[UIntPtr]::Zero) }
