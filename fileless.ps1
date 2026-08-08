Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "     Fileless tool, By Loyal                  " -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

$reportPath = Join-Path $PSScriptRoot "Fileless_Scan_Results.txt"
$timestamp  = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
"Fileless tool, By Loyal Scan Report" | Out-File -FilePath $reportPath -Encoding utf8
"Generated: $timestamp" | Add-Content -Path $reportPath
"=====================================" | Add-Content -Path $reportPath
"" | Add-Content -Path $reportPath

Write-Host "[*] Récupération de l'historique complet des commandes..." -ForegroundColor Yellow
"[HISTORIQUE COMPLET DES COMMANDES]" | Add-Content -Path $reportPath
"------------------------------------" | Add-Content -Path $reportPath

$AllCmds = @()

$PsEvents = Get-WinEvent -FilterHashtable @{
    LogName = 'Microsoft-Windows-PowerShell/Operational'
    Id      = 4104
} -MaxEvents 1000 -ErrorAction SilentlyContinue

foreach ($ev in $PsEvents) {
    $text = ($ev.Properties[2].Value -replace "`r`n|`r|`n", " ").Trim()
    if ($text.Length -lt 5) { continue }
    if ($text -eq '$global:?') { continue }
    $preview = if ($text.Length -gt 300) { $text.Substring(0, 300) + "..." } else { $text }
    $AllCmds += [PSCustomObject]@{
        Date    = $ev.TimeCreated
        Source  = "PowerShell"
        Command = $preview
    }
}

$CmdEvents = Get-WinEvent -FilterHashtable @{
    LogName = 'Security'
    Id      = 4688
} -MaxEvents 1000 -ErrorAction SilentlyContinue

foreach ($ev in $CmdEvents) {
    $proc    = $ev.Properties[5].Value
    $cmdLine = $ev.Properties[8].Value
    if (-not $cmdLine -or $cmdLine.Length -lt 3) { continue }
    if ($proc -match 'svchost|csrss|wininit|smss|lsass|RuntimeBroker|SearchHost') { continue }
    $AllCmds += [PSCustomObject]@{
        Date    = $ev.TimeCreated
        Source  = "CMD/Process [$([System.IO.Path]::GetFileName($proc))]"
        Command = $cmdLine.Trim()
    }
}

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

Write-Host ""
Write-Host "Do you want to analyze RAM to detect PE injection / shellcode? (Y/N): " -NoNewline -ForegroundColor Cyan
$analyzeRam = Read-Host

if ($analyzeRam -match "^[Yy]") {
    Write-Host "`n[*] Loading Win32 memory API..." -ForegroundColor Cyan

    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class MemAPI {
    [StructLayout(LayoutKind.Sequential)]
    public struct MEMORY_BASIC_INFORMATION {
        public IntPtr BaseAddress;
        public IntPtr AllocationBase;
        public uint   AllocationProtect;
        public IntPtr RegionSize;
        public uint   State;
        public uint   Protect;
        public uint   Type;
    }

    [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, int dwProcessId);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr hObject);
    [DllImport("kernel32.dll")] public static extern int VirtualQueryEx(IntPtr hProcess, IntPtr lpAddress, out MEMORY_BASIC_INFORMATION lpBuffer, uint dwLength);
    [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int nSize, out int lpNumberOfBytesRead);
    [DllImport("psapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern uint GetMappedFileNameW(IntPtr hProcess, IntPtr lpv, StringBuilder lpFilename, uint nSize);
}
"@ -ErrorAction SilentlyContinue

    $PROCESS_VM_READ        = 0x0010
    $PROCESS_QUERY_INFO     = 0x0400
    $MEM_COMMIT             = 0x1000
    $MEM_PRIVATE            = 0x20000
    $PAGE_EXECUTE           = 0x10
    $PAGE_EXECUTE_READ      = 0x20
    $PAGE_EXECUTE_READWRITE = 0x40
    $PAGE_EXECUTE_WRITECOPY = 0x80

    $execProtections = @($PAGE_EXECUTE, $PAGE_EXECUTE_READ, $PAGE_EXECUTE_READWRITE, $PAGE_EXECUTE_WRITECOPY)

    $selfPid   = $PID
    $processes = Get-Process | Where-Object { $_.Id -ne $selfPid -and $_.Id -ne 0 -and $_.Id -ne 4 }

    Write-Host "[*] Scanning $($processes.Count) processes for anonymous executable memory..." -ForegroundColor Yellow
    "" | Add-Content -Path $reportPath
    "[RAM MEMORY SCAN]" | Add-Content -Path $reportPath
    "------------------" | Add-Content -Path $reportPath

    $findingsCount = 0

    foreach ($proc in $processes) {
        $hProcess = [MemAPI]::OpenProcess($PROCESS_VM_READ -bor $PROCESS_QUERY_INFO, $false, $proc.Id)
        if ($hProcess -eq [IntPtr]::Zero) { continue }

        $address = [IntPtr]::Zero
        $mbi     = New-Object MemAPI+MEMORY_BASIC_INFORMATION
        $mbiSize = [Runtime.InteropServices.Marshal]::SizeOf($mbi)

        while ([MemAPI]::VirtualQueryEx($hProcess, $address, [ref]$mbi, [uint32]$mbiSize) -ne 0) {
            $sizeBytes   = $mbi.RegionSize.ToInt64()
            $isCommitted = ($mbi.State  -eq $MEM_COMMIT)
            $isPrivate   = ($mbi.Type   -eq $MEM_PRIVATE)
            $isExec      = $execProtections -contains $mbi.Protect
            $isRWX       = ($mbi.Protect -eq $PAGE_EXECUTE_READWRITE)

            if ($isCommitted -and $isPrivate -and $isExec) {
                $mappedName   = New-Object System.Text.StringBuilder 512
                $mappedResult = [MemAPI]::GetMappedFileNameW($hProcess, $mbi.BaseAddress, $mappedName, 512)
                $isFileBacked = ($mappedResult -gt 0)

                if (-not $isFileBacked) {
                    $hasMZ = $false
                    $hasPE = $false

                    $buf64     = New-Object byte[] 64
                    $bytesRead = 0
                    if ([MemAPI]::ReadProcessMemory($hProcess, $mbi.BaseAddress, $buf64, 64, [ref]$bytesRead)) {
                        if ($bytesRead -ge 2 -and $buf64[0] -eq 0x4D -and $buf64[1] -eq 0x5A) {
                            $hasMZ = $true
                            if ($bytesRead -ge 0x40) {
                                $eLfanew = [BitConverter]::ToInt32($buf64, 0x3C)
                                if ($eLfanew -gt 0 -and $eLfanew -lt 0x400) {
                                    $peSig       = New-Object byte[] 4
                                    $peBytesRead = 0
                                    $peAddr      = [IntPtr]($mbi.BaseAddress.ToInt64() + $eLfanew)
                                    if ([MemAPI]::ReadProcessMemory($hProcess, $peAddr, $peSig, 4, [ref]$peBytesRead)) {
                                        if ($peBytesRead -ge 4 -and $peSig[0] -eq 0x50 -and $peSig[1] -eq 0x45 -and $peSig[2] -eq 0x00 -and $peSig[3] -eq 0x00) {
                                            $hasPE = $true
                                        }
                                    }
                                }
                            }
                        }
                    }

                    $isConfirmedPE   = $hasMZ -and $hasPE
                    $isSuspiciousRWX = $isRWX -and ($sizeBytes -ge 65536)

                    if ($isConfirmedPE -or $isSuspiciousRWX) {
                        $findingsCount++
                        $addrHex  = "0x{0:X}" -f $mbi.BaseAddress.ToInt64()
                        $sizeKB   = [math]::Round($sizeBytes / 1KB, 1)
                        $protName = switch ($mbi.Protect) {
                            0x10 { "PAGE_EXECUTE" }
                            0x20 { "PAGE_EXECUTE_READ" }
                            0x40 { "PAGE_EXECUTE_READWRITE (RWX)" }
                            0x80 { "PAGE_EXECUTE_WRITECOPY" }
                            default { "0x{0:X}" -f $mbi.Protect }
                        }
                        $finding = if ($isConfirmedPE) {
                            "PE INJECTION CONFIRMED (MZ + PE signature in anonymous private memory)"
                        } else {
                            "Large anonymous RWX region (>= 64 KB, not file-backed)"
                        }
                        $line = "[!] $finding`n    Process : $($proc.ProcessName) (PID $($proc.Id))`n    Address : $addrHex`n    Size    : $sizeKB KB`n    Protect : $protName"
                        Write-Host $line -ForegroundColor Red
                        Add-Content -Path $reportPath -Value $line
                        Add-Content -Path $reportPath -Value ""
                    }
                }
            }

            try {
                $next    = $mbi.BaseAddress.ToInt64() + $sizeBytes
                $address = [IntPtr]$next
            } catch { break }
            if ($address.ToInt64() -le 0) { break }
        }

        [MemAPI]::CloseHandle($hProcess) | Out-Null
    }

    if ($findingsCount -eq 0) {
        Write-Host "[+] Memory scan complete. No PE injection or suspicious anonymous RWX detected." -ForegroundColor Green
        Add-Content -Path $reportPath -Value "[+] Memory scan complete. No PE injection or suspicious anonymous RWX detected."
    } else {
        Write-Host "`n[!] $findingsCount finding(s). See report for details." -ForegroundColor Red
        Add-Content -Path $reportPath -Value "[!] $findingsCount finding(s)."
    }
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
