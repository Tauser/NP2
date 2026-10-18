param(
    [string]$ToolName = 'fusion_mcp_execute',
    [string]$ArgumentsPath,
    [string]$ScriptPath,
    [switch]$ReadOnly,
    [string]$OutputPath,
    [int]$TimeoutSec = 180
)
$ErrorActionPreference = 'Stop'
$endpoint = 'http://127.0.0.1:27182/mcp'
$headers = @{ Accept = 'application/json, text/event-stream' }
$init = @{jsonrpc='2.0';id=1;method='initialize';params=@{protocolVersion='2024-11-05';capabilities=@{};clientInfo=@{name='NP2-CAD';version='1.0'}}} | ConvertTo-Json -Depth 20
$response = Invoke-WebRequest -Uri $endpoint -Method Post -ContentType 'application/json' -Headers $headers -Body $init -TimeoutSec 20
$headers['MCP-Session-Id'] = $response.Headers['MCP-Session-Id'][0]
$notification = @{jsonrpc='2.0';method='notifications/initialized'} | ConvertTo-Json
$null = Invoke-WebRequest -Uri $endpoint -Method Post -ContentType 'application/json' -Headers $headers -Body $notification -TimeoutSec 20
if ($ScriptPath) {
    $arguments = @{featureType='script';object=@{script=(Get-Content -Raw -LiteralPath $ScriptPath);readOnly=[bool]$ReadOnly}}
} else {
    $arguments = Get-Content -Raw -LiteralPath $ArgumentsPath | ConvertFrom-Json -AsHashtable
}
$body = @{jsonrpc='2.0';id=2;method='tools/call';params=@{name=$ToolName;arguments=$arguments}} | ConvertTo-Json -Depth 30 -Compress
$response = Invoke-WebRequest -Uri $endpoint -Method Post -ContentType 'application/json; charset=utf-8' -Headers $headers -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) -TimeoutSec $TimeoutSec
if ($OutputPath) { $response.Content | Set-Content -LiteralPath $OutputPath -Encoding utf8 }
$response.Content
