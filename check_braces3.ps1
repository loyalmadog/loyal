$content = Get-Content 'src\main.cpp' -Raw
$depth = 0
$lineNum = 0
$inString = $false
foreach($line in ($content -split "`n")) {
    $lineNum++
    $chars = $line.ToCharArray()
    for ($i = 0; $i -lt $chars.Length; $i++) {
        $c = $chars[$i]
        if ($c -eq '/' -and $i+1 -lt $chars.Length -and $chars[$i+1] -eq '/') { break }
        if ($c -eq '"') { $inString = !$inString }
        if (!$inString) {
            if ($c -eq '{') { $depth++ }
            elseif ($c -eq '}') { $depth-- }
        }
    }
    if ($lineNum -eq 1071) { Write-Host "Depth at 1071: $depth" }
    if ($lineNum -eq 1072) { Write-Host "Depth at 1072: $depth" }
    if ($lineNum -ge 795 -and $lineNum -le 805) { Write-Host "Line $lineNum d=$depth : $($line.Trim())" }
}
Write-Host "Final depth: $depth"
