$ErrorActionPreference = 'Stop'
$payload = [Text.Encoding]::ASCII.GetBytes('NP2 OTA image stream test: chunks are bounded and exact.')
$expected = '2f283ef73d59a91a41236708b6764fe0058e6c18c8cb4bff48165650ba0e28e7'
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $offset = 0
    while ($offset -lt $payload.Length) {
        $count = [Math]::Min(7, $payload.Length - $offset)
        [void]$sha256.TransformBlock($payload, $offset, $count, $null, 0)
        $offset += $count
    }
    [void]$sha256.TransformFinalBlock([byte[]]::new(0), 0, 0)
    $actual = -join ($sha256.Hash | ForEach-Object { $_.ToString('x2') })
    if ($actual -ne $expected) { throw "SHA-256 vector mismatch: $actual" }
} finally {
    $sha256.Dispose()
}
Write-Output 'update_image_hash_vector_test: chunked SHA-256 vector passed'
