[CmdletBinding()]
param(
    [ValidateSet('CACHE_CORRUPT_NEWEST', 'CHECK', 'CHECK_DHCP_TIMEOUT', 'CHECK_NXDNS', 'CHECK_DNS_TIMEOUT', 'CHECK_HTTPS_OVERSIZE', 'CHECK_HTTPS_TIMEOUT', 'CHECK_TLS_REJECT', 'CONFIG_CORRUPT_NEWEST', 'CONFIG_WRITE', 'FORGET', 'RECOVER_C6', 'RECOVER_C6_COOLDOWN', 'RECOVER_C6_COOLDOWN_FULL', 'REFRESH_OFFLINE_DATA', 'OTA_STATUS', 'OTA_PREFLIGHT', 'OTA_APPLY', 'OPEN')]
    [string]$Request = 'CHECK',

    [string]$Ssid,

    [ValidatePattern('^COM[0-9]+$')]
    [string]$Port = 'COM8',

    [ValidateRange(100, 480000)]
    [int]$WaitMilliseconds = 300,

    [ValidateRange(0, 5000)]
    [int]$SettleMilliseconds = 750
)

$serialPort = [System.IO.Ports.SerialPort]::new(
    $Port,
    115200,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serialPort.NewLine = "`n"
$serialPort.ReadTimeout = 1500
$serialPort.WriteTimeout = 1500

if ($Request -eq 'OPEN') {
    $hasNonPrintableCharacter = $false
    if (-not [string]::IsNullOrEmpty($Ssid)) {
        foreach ($character in $Ssid.ToCharArray()) {
            $codePoint = [int][char]$character
            if ($codePoint -lt 0x20 -or $codePoint -gt 0x7e) {
                $hasNonPrintableCharacter = $true
                break
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($Ssid) -or $Ssid.Length -gt 32 -or $hasNonPrintableCharacter) {
        throw 'OPEN requires a printable SSID of 1 to 32 characters.'
    }
    $wireRequest = "OPEN $Ssid"
} else {
    $wireRequest = $Request
}

try {
    $serialPort.Open()
    Start-Sleep -Milliseconds $SettleMilliseconds
    [byte[]]$wireBytes = [System.Text.Encoding]::ASCII.GetBytes($wireRequest + "`n")
    $serialPort.BaseStream.Write($wireBytes, 0, $wireBytes.Length)
    $serialPort.BaseStream.Flush()
    $replyBuilder = [System.Text.StringBuilder]::new()
    $deadline = [DateTime]::UtcNow.AddMilliseconds($WaitMilliseconds)
    do {
        Start-Sleep -Milliseconds 100
        $chunk = $serialPort.ReadExisting()
        if (-not [string]::IsNullOrEmpty($chunk)) {
            [void]$replyBuilder.Append($chunk)
        }
        $campaignComplete = $Request -eq 'RECOVER_C6_COOLDOWN' -and
            ($replyBuilder.ToString().Contains('Hosted cooldown campaign passed') -or
             $replyBuilder.ToString().Contains('Hosted cooldown campaign ended'))
        $fullCooldownComplete = $Request -eq 'RECOVER_C6_COOLDOWN_FULL' -and
            ($replyBuilder.ToString().Contains('Hosted full cooldown campaign passed') -or
             $replyBuilder.ToString().Contains('Hosted cooldown campaign ended'))
        $dhcpComplete = $Request -eq 'CHECK_DHCP_TIMEOUT' -and
            ($replyBuilder.ToString().Contains('DHCP silence injection passed') -or
             $replyBuilder.ToString().Contains('DHCP silence injection setup failed'))
        if ($campaignComplete -or $fullCooldownComplete -or $dhcpComplete) {
            break
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    $reply = $replyBuilder.ToString()
    if ([string]::IsNullOrWhiteSpace($reply)) {
        Write-Output 'No immediate reply. Confirm that the physical USB window is armed and the panel has IP when using CHECK.'
    } else {
        Write-Output $reply.Trim()
    }
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
    $serialPort.Dispose()
}
