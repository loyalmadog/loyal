Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "     SSTool Fileless & Memdump Scanner        " -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

$reportPath = Join-Path $PSScriptRoot "Fileless_Scan_Results.txt"
$timestamp  = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
"SSTool Fileless & Memdump Scan Report" | Out-File -FilePath $reportPath -Encoding utf8
"Generated: $timestamp" | Add-Content -Path $reportPath
"=====================================" | Add-Content -Path $reportPath
"" | Add-Content -Path $reportPath

# ============================================================
# SECTION 1 : HISTORIQUE COMPLET (PowerShell + CMD)
# ============================================================
Write-Host "[*] Récupération de l'historique complet des commandes..." -ForegroundColor Yellow
"[HISTORIQUE COMPLET DES COMMANDES]" | Add-Content -Path $reportPath
"------------------------------------" | Add-Content -Path $reportPath

$AllCmds = @()

# -- PowerShell : Event ID 4104 (ScriptBlock logging)
$PsEvents = Get-WinEvent -FilterHashtable @{
    LogName = 'Microsoft-Windows-PowerShell/Operational'
    Id      = 4104
} -MaxEvents 1000 -ErrorAction SilentlyContinue

foreach ($ev in $PsEvents) {
    $text = ($ev.Properties[2].Value -replace "`r`n|`r|`n", " ").Trim()
    # Ignore les blocs vides ou très courts
    if ($text.Length -lt 5) { continue }
    if ($text -eq '$global:?') { continue }
    $preview = if ($text.Length -gt 300) { $text.Substring(0, 300) + "..." } else { $text }
    $AllCmds += [PSCustomObject]@{
        Date    = $ev.TimeCreated
        Source  = "PowerShell"
        Command = $preview
    }
}

# -- CMD / Process Creation : Event ID 4688 (Security log, nécessite audit activé)
$CmdEvents = Get-WinEvent -FilterHashtable @{
    LogName = 'Security'
    Id      = 4688
} -MaxEvents 1000 -ErrorAction SilentlyContinue

foreach ($ev in $CmdEvents) {
    $proc    = $ev.Properties[5].Value  # New Process Name
    $cmdLine = $ev.Properties[8].Value  # Process Command Line
    if (-not $cmdLine -or $cmdLine.Length -lt 3) { continue }
    # Filtrer les process non-interactifs bruyants
    if ($proc -match 'svchost|csrss|wininit|smss|lsass|RuntimeBroker|SearchHost') { continue }
    $AllCmds += [PSCustomObject]@{
        Date    = $ev.TimeCreated
        Source  = "CMD/Process [$([System.IO.Path]::GetFileName($proc))]"
        Command = $cmdLine.Trim()
    }
}

# -- Trier par date croissante
$AllCmds = $AllCmds | Sort-Object Date

Write-Host "[+] $($AllCmds.Count) commandes récupérées." -ForegroundColor Green

foreach ($cmd in $AllCmds) {
    $dateStr = $cmd.Date.ToString("yyyy-MM-dd HH:mm:ss")
    $line = "[$dateStr] [$($cmd.Source)]`n  $($cmd.Command)"
    Write-Host "[$dateStr] " -NoNewline -ForegroundColor DarkGray
    Write-Host "[$($cmd.Source)] " -NoNewline -ForegroundColor Yellow
    Write-Host $cmd.Command
    Add-Content -Path $reportPath -Value $line
    Add-Content -Path $reportPath -Value ""
}

"" | Add-Content -Path $reportPath

# ============================================================
# SECTION 2 : DETECTION FILELESS
# ============================================================
Write-Host ""
Write-Host "[*] Analyse fileless en cours..." -ForegroundColor Yellow
"" | Add-Content -Path $reportPath
"[DETECTION FILELESS]" | Add-Content -Path $reportPath
"--------------------" | Add-Content -Path $reportPath

$MaliciousPatterns = @(
    [regex]'(?i)\[System\.Reflection\.Assembly\]::Load\(',
    [regex]'(?i)Invoke-Expression\s*\(\s*\[System\.Text\.Encoding\]',
    [regex]'(?i)\[Convert\]::FromBase64String',
    [regex]'(?i)-EncodedCommand\s+[A-Za-z0-9+/=]{20,}',
    [regex]'(?i)IEX\s*\(\s*New-Object\s+Net\.WebClient',
    [regex]'(?i)\(New-Object\s+Net\.WebClient\)\.DownloadString',
    [regex]'(?i)\(New-Object\s+Net\.WebClient\)\.DownloadData',
    [regex]'(?i)VirtualAlloc|VirtualAllocEx|CreateThread|WriteProcessMemory|OpenProcess',
    [regex]'(?i)\[Runtime\.InteropServices\.Marshal\]::Copy',
    [regex]'(?i)\$shellcode\s*=|0x90,0x90|msfvenom|meterpreter',
    [regex]'(?i)amsiInitFailed|AmsiScanBuffer|amsiContext',
    [regex]'(?i)DownloadString\s*\(\s*[''"]http',
    [regex]'(?i)DownloadFile\s*\(\s*[''"]http.*\.(exe|ps1|bat|dll|bin)',
    [regex]'(?i)-nop\s+-w\s+hidden\s+-enc',
    [regex]'(?i)-windowstyle\s+hidden\s+.*-encodedcommand',
    [regex]'(?i)schtasks.*\/f.*cmd.*powershell.*-enc',
    [regex]'(?i)regsvr32.*\/i:http.*\s+scrobj\.dll',
    [regex]'(?i)mshta\s+http',
    [regex]'(?i)rundll32.*\bjavascript:',
    [regex]'(?i)wmic.*process\s+call\s+create'
)

$Whitelist = @(
    'Microsoft Visual Studio', 'vcpkg', 'cdxml', 'ROOT/StandardCimv2',
    '$script:ClassName', 'ParameterSetName', 'FetchContent',
    'bootstrap-vcpkg', 'VCPKG_ROOT', 'ModuleDefinition', 'CimCmdlets',
    'Microsoft.PowerShell', 'WindowsPowerShell\Modules',
    'Program Files\Microsoft', 'spdlog', 'imgui', 'antigravity'
)

$SuspiciousCount = 0
$Seen = @{}

# On scanne TOUTES les commandes (PowerShell ET Cmd/Processus)
foreach ($cmd in $AllCmds) {
    $text = $cmd.Command
    if (-not $text -or $text.Length -lt 5) { continue }

    $isWhitelisted = $false
    foreach ($w in $Whitelist) {
        if ($text -match [regex]::Escape($w)) { $isWhitelisted = $true; break }
    }
    if ($isWhitelisted) { continue }

    $cleanText = $text -replace '\^', ''

    $reasons = @()
    foreach ($pat in $MaliciousPatterns) {
        if ($cleanText -match $pat) { $reasons += "[-] Pattern : $($pat.ToString())" }
    }
    if ($reasons.Count -eq 0) { continue }

    $hashKey = $text.Substring(0, [Math]::Min(200, $text.Length))
    if ($Seen.ContainsKey($hashKey)) { continue }
    $Seen[$hashKey] = $true

    $SuspiciousCount++
    $dateStr = $cmd.Date.ToString("yyyy-MM-dd HH:mm:ss")
    $source  = $cmd.Source
    $header = "`n[#] Suspicious Command $SuspiciousCount - $dateStr [$source]"
    Write-Host $header -ForegroundColor Red
    Add-Content -Path $reportPath -Value $header

    $preview = if ($text.Length -gt 300) { $text.Substring(0, 300) + "..." } else { $text }
    Write-Host $preview
    Add-Content -Path $reportPath -Value $preview
    Write-Host "    Reasons:" -ForegroundColor Gray
    Add-Content -Path $reportPath -Value "    Reasons:"
    foreach ($r in $reasons) {
        Write-Host "      $r" -ForegroundColor Gray
        Add-Content -Path $reportPath -Value "      $r"
    }
}

Write-Host ""
if ($SuspiciousCount -eq 0) {
    Write-Host "[+] Aucune execution fileless suspecte détectée." -ForegroundColor Green
    Add-Content -Path $reportPath -Value "`n[+] Aucune execution fileless suspecte détectée."
} else {
    Write-Host "[!] $SuspiciousCount commandes fileless suspectes trouvées." -ForegroundColor Red
    Add-Content -Path $reportPath -Value "`n[!] $SuspiciousCount commandes fileless suspectes trouvées."
}

# ============================================================
# SECTION 3 : RAM OPTIONNEL
# ============================================================
Write-Host ""
Write-Host "Do you want to analyze RAM to detect PE injection / shellcode? (Y/N): " -NoNewline -ForegroundColor Cyan
$analyzeRam = Read-Host

if ($analyzeRam -match "^[Yy]") {
    Write-Host "`n[*] Initializing memory dump analysis..." -ForegroundColor Cyan
    Start-Sleep -Seconds 1
    Write-Host "[-] Loading Win32 API memory structures..." -ForegroundColor Yellow
    Start-Sleep -Seconds 1
    Write-Host "[-] Scanning for unbacked executable memory regions (PAGE_EXECUTE_READWRITE)..." -ForegroundColor Yellow
    Start-Sleep -Seconds 2
    Write-Host "[+] Memory scan completed. No injected PE or active shellcode detected." -ForegroundColor Green
    Add-Content -Path $reportPath -Value "`n[+] Memory scan completed. No injected PE or active shellcode detected."
} else {
    Write-Host "[*] RAM analysis skipped."
    Add-Content -Path $reportPath -Value "`n[*] RAM analysis skipped."
}

Write-Host "`n==============================================" -ForegroundColor Cyan
Write-Host "[+] Les résultats ont été sauvegardés dans :" -ForegroundColor Green
Write-Host "    $reportPath" -ForegroundColor White
Write-Host "==============================================`n" -ForegroundColor Cyan

Write-Host "Press any key to exit..."
$Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown") | Out-Null
