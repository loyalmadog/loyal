$content = Get-Content 'src\main.cpp'
$depth = 0
$inStr = $false
$inChar = $false

for ($i=0; $i -lt $content.Count; $i++) {
    $line = $content[$i]
    $j = 0
    while ($j -lt $line.Length) {
        $c = $line[$j]
        
        # Skip comments
        if (!$inStr -and !$inChar -and $c -eq '/' -and $j+1 -lt $line.Length -and $line[$j+1] -eq '/') {
            break
        }
        
        # Escape character
        if ($c -eq '\') {
            $j += 2
            continue
        }
        
        # String toggle
        if ($c -eq '"' -and !$inChar) {
            $inStr = !$inStr
        }
        
        # Char toggle
        if ($c -eq "'" -and !$inStr) {
            $inChar = !$inChar
        }
        
        # Brace counting
        if (!$inStr -and !$inChar) {
            if ($c -eq '{') {
                $depth++
            } elseif ($c -eq '}') {
                $depth--
                if ($depth -lt 0) {
                    Write-Host "Depth went below zero at line $($i+1)"
                    exit
                }
            }
        }
        $j++
    }
}
Write-Host "Final depth: $depth"
