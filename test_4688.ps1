$ev = Get-WinEvent -FilterHashtable @{LogName='Security'; Id=4688} -MaxEvents 1 -ErrorAction SilentlyContinue
if ($ev) {
    for($i=0; $i -lt $ev.Properties.Count; $i++){ 
        Write-Host "$i : $($ev.Properties[$i].Value)" 
    }
} else {
    Write-Host "No 4688 event found."
}
