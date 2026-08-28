$procs = Get-Process matlab -ErrorAction SilentlyContinue
foreach ($p in $procs) {
    "{0}  mem={1:N2}GB" -f $p.Id, ($p.WorkingSet64/1GB)
}
""
"--- 3 秒采样 CPU%（>1% 才显示）---"
$c = Get-Counter "\Process(matlab*)\% Processor Time" -SampleInterval 3 -MaxSamples 1 -ErrorAction SilentlyContinue
foreach ($s in $c.CounterSamples) {
    if ($s.CookedValue -gt 1) { "{0}  {1:N0}%" -f $s.InstanceName, $s.CookedValue }
}
