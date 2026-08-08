


[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "[?] Checking USN Journal at path: C:\`$Extend\`$UsnJrnl:`$J"


try {
    $bootTime = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
} catch {
    Write-Host "[-] Failed to retrieve boot time." -ForegroundColor Red
    
    $uptimeMs = [Environment]::TickCount64
    $bootTime = (Get-Date).AddMilliseconds(-$uptimeMs)
}


try {
    
    $fsutil = fsutil usn queryjournal C: 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[-] Failed to query USN Journal. Ensure you are running as Administrator." -ForegroundColor Red
    } else {
        $firstLine = $fsutil | Select-Object -First 1
        if ($firstLine -match '0x([0-9a-fA-F]{16})') {
            $hexId = $Matches[1]
            $decId = [Convert]::ToUInt64($hexId, 16)
            $usnCreationTime = [datetime]::FromFileTime($decId)
            
            $usnStr = $usnCreationTime.ToString('yyyy-MM-dd HH:mm:ss')
            $bootStr = $bootTime.ToString('yyyy-MM-dd HH:mm:ss')
            
            Write-Host "[!] USN Journal creation time: $usnStr"
            Write-Host "[#] System boot time: $bootStr"
            Write-Host ""
            
            if ($usnCreationTime -le $bootTime) {
                Write-Host "[#] The USN Journal is Intact!" -ForegroundColor Green
                Write-Host "   Explanation: The USN Journal creation time ($usnStr) is BEFORE the boot time ($bootStr), indicating no recent changes or deletions."
            } else {
                Write-Host "[!] `$UsnJrnl:`$J Suspicious!" -ForegroundColor Red
                Write-Host "   Explanation: The USN Journal creation time ($usnStr) is AFTER the boot time ($bootStr), indicating it was recreated recently. This is highly suspicious."
            }
        } else {
            Write-Host "[-] Failed to retrieve USN Journal ID from fsutil output." -ForegroundColor Red
        }
    }
} catch {
    Write-Host "[-] Error: $_" -ForegroundColor Red
}

Write-Host ""

if ($host.Name -eq "ConsoleHost") {
    Write-Host "Appuyez sur n'importe quelle touche pour fermer..."
    [void]$Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
} else {
    Read-Host "Appuyez sur Entree pour fermer..."
}
