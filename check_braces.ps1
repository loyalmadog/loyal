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
    
    if ($lineNum -ge 295 -and $lineNum -le 320) {
        $trim = $line.Trim()
        if ($trim.Length -gt 60) { $trim = $trim.Substring(0,60) }
        Write-Host "Line $lineNum d=$depth : $trim"
    }
}
