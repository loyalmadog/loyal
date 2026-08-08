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
    if ($depth -le 0 -and $lineNum -lt 1150 -and $lineNum -gt 20) {
        Write-Host "Depth hit $depth at line $lineNum"
    }
}
