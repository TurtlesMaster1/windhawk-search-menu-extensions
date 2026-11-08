Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$root=[System.Windows.Automation.AutomationElement]::RootElement
$pidC=[int]$args[0]
$cond=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty,$pidC)
$wins=$root.FindAll([System.Windows.Automation.TreeScope]::Children,$cond)
foreach($w in $wins){
  "WINDOW: $($w.Current.Name) [$($w.Current.ClassName)] $($w.Current.BoundingRectangle)"
  $all=$w.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
  foreach($e in $all){ $n=$e.Current.Name; if($n){ "  {0} | {1} | {2}" -f $e.Current.ControlType.ProgrammaticName,$n,$e.Current.AutomationId } }
}
