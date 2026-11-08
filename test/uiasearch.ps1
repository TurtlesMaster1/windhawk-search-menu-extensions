# Drive the composer search purely through UI Automation (no synthetic input).
param([string]$text = 'wh t', [string]$item = '', [string]$how = 'auto', [switch]$shot, [string]$button = '')
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Windows.Forms,System.Drawing
$A=[System.Windows.Automation.AutomationElement]; $TS=[System.Windows.Automation.TreeScope]
$root=$A::RootElement
function ByClass($parent,$cls,$scope){ $parent.FindFirst($scope,(New-Object System.Windows.Automation.PropertyCondition($A::ClassNameProperty,$cls))) }
function ById($parent,$id){ $parent.FindFirst($TS::Descendants,(New-Object System.Windows.Automation.PropertyCondition($A::AutomationIdProperty,$id))) }
$comp=ByClass $root 'ShellUXFrameWindowClass' $TS::Children
if(-not $comp){
  $tray=ByClass $root 'Shell_TrayWnd' $TS::Children
  $btn=ById $tray 'ComposerControlTaskbarUx'
  if(-not $btn){ 'taskbar search element not found'; exit }
  $o=$null
  if($btn.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern,[ref]$o)){ $o.Invoke() }
  elseif($btn.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern,[ref]$o)){ $o.Toggle() }
  else { "search element patterns: " + (($btn.GetSupportedPatterns()|%{$_.ProgrammaticName}) -join ','); exit }
  for($i=0;$i -lt 30 -and -not $comp;$i++){ Start-Sleep -Milliseconds 100; $comp=ByClass $root 'ShellUXFrameWindowClass' $TS::Children }
  if(-not $comp){ 'composer did not open'; exit }
}
$edit=ById $comp 'Entry'
$o=$null
if($edit.TryGetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern,[ref]$o)){ $o.SetValue($text) } else { 'no ValuePattern on Entry'; exit }
Start-Sleep -Milliseconds 2000
function Dump($tag){ foreach($e in $comp.FindAll($TS::Descendants,[System.Windows.Automation.Condition]::TrueCondition)){ $n=$e.Current.Name; if($n){ "  {0}{1} | {2} | {3}" -f $tag,$e.Current.ControlType.ProgrammaticName.Replace('ControlType.',''),$n,$e.Current.AutomationId } } }
Dump ''
if($shot){ try{ $b=[System.Windows.Forms.Screen]::PrimaryScreen.Bounds; $bmp=New-Object System.Drawing.Bitmap $b.Width,$b.Height; $g=[System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location,[System.Drawing.Point]::Empty,$b.Size); $bmp.Save("$env:TEMP\wse-look.png"); 'shot saved' } catch { "shot failed: $_" } }
if($item){
  $t=$null
  foreach($e in $comp.FindAll($TS::Descendants,[System.Windows.Automation.Condition]::TrueCondition)){ if($e.Current.ControlType -eq [System.Windows.Automation.ControlType]::ListItem -and $e.Current.Name -like "$item*"){ $t=$e; break } }
  if(-not $t){ "item '$item' not found" } else {
    "target patterns: " + (($t.GetSupportedPatterns()|%{$_.ProgrammaticName}) -join ',')
    $kids=@($t)+@($t.FindAll($TS::Descendants,[System.Windows.Automation.Condition]::TrueCondition))
    foreach($k in $kids){
      $o=$null
      if(($how -eq 'auto' -or $how -eq 'toggle') -and $k.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern,[ref]$o)){ $o.Toggle(); "toggled ($($k.Current.ControlType.ProgrammaticName))"; break }
      if(($how -eq 'auto' -or $how -eq 'invoke') -and $k.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern,[ref]$o)){ $o.Invoke(); "invoked ($($k.Current.ControlType.ProgrammaticName) '$($k.Current.Name)')"; break }
      if($how -eq 'select' -and $k.TryGetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern,[ref]$o)){ $o.Select(); "selected"; break }
    }
    Start-Sleep -Milliseconds 1500
    $comp=ByClass $root 'ShellUXFrameWindowClass' $TS::Children
    if($comp -and $button){
      $bt=$null; foreach($e in $comp.FindAll($TS::Descendants,[System.Windows.Automation.Condition]::TrueCondition)){ if($e.Current.ControlType -eq [System.Windows.Automation.ControlType]::Button -and $e.Current.Name -eq $button){ $bt=$e; break } }
      if(-not $bt){ Start-Sleep -Milliseconds 2500; foreach($e in $comp.FindAll($TS::Descendants,[System.Windows.Automation.Condition]::TrueCondition)){ if($e.Current.ControlType -eq [System.Windows.Automation.ControlType]::Button -and $e.Current.Name -eq $button){ $bt=$e; break } } }
      if($bt){ $o=$null; if($bt.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern,[ref]$o)){ $o.Invoke(); "pressed '$button'" } ; Start-Sleep -Milliseconds 1500 } else { "button '$button' not found" }
      $comp=ByClass $root 'ShellUXFrameWindowClass' $TS::Children
    }
    if($comp){ Dump 'after: ' } else { 'composer closed after action' }
  }
}
