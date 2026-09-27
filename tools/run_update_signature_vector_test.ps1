param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-update-signature-vector-test'))
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$manifest = [Convert]::FromBase64String('TTJQTgEAaAABAAAAAAAAAAAAAAAAAAAAAQAAAAEAAAACAAAAZADHAAcAAAAAAAAAAACAAAEAAQADAAAABgACAaUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=')
$publicKey = [Convert]::FromBase64String('MIIBojANBgkqhkiG9w0BAQEFAAOCAY8AMIIBigKCAYEA1Mr7WoypAEtqUoniuTe1jtk9Ka5WOJZoFR/jOwLCPta/RKawhkc3HFwiOtLNdwx+Wrjk/iNCxL9GsQIShQ1iuX5ecMPYuZwdt2AXt5JpkQwbdmx19xVZNvBeEhmBEE8sLRAyZkE01jkQ6oCT6oXZ31e1KPYpmfPnk91xl4ij87LWunZI7IDxrFkszvANyDfETeQaYcHA/Id4hsSbVqmGysnhjcu6zFO7e+opY9OvvlSFeqgiQfWsLQ2j4TL6Fnzc0n2jbUpToE7No1Qkrh7f6jp/ej+Ty/9lnWjXgc7P3BudV5uJFBzMourH9+oWorVhgnT0pAsVmv4w647Gv+DPRyf6yRtdU+Mv9vU4RAjwAlJYnfRfbusXhLyO/TMpQVt5Nc7njvO6mK6Lit2E8Ep3r5JoBb0XZlv45uybRQFESvu+FwIpm2YUAZhN+mruB/q0ngHqF7o4I6WAfsQgzBtMjHVrQFtkWlFEQP3BSwyAdPDrcMI7O5Rdsk6fw+h5U9RtAgMBAAE=')
$signature = [Convert]::FromBase64String('UrNMViOzm0j7mdzdnPAhit4w/QR3E+Ubthg5Ly2xvb+KlP6Z4zqdi2/kbLaQ8VlUbPXknYo6yfrgXiY4aZV260xl+M1Gg3YfdJ8d6YXVnYGyOKYXRtlUpgKKLeLymtSJuFjhG9dVmDOhzvbhutn8BtNYivlW3ErJ4jNfnKJujv/ETg4jDkJhfIXplaeEz0oPfuOiIxzq7HMNQCMgqUnGoX7bvTZ1y+9QQGoairxXFbxchiPShntazoDYKdpBp/p8Z6BOVrXXszTuXO39xn4MfLxZBVzdJGiBY1BBOxxeo9HiwnPMvGrn8NIXoKb/rfiiASky3TKwpLPMlsPC5vfYYiB4us/tyowqszEkwLD2wH7N5j3Xbmb0hDB/evG1iC+AY+iLiQfGey2bmWNoYtBn+BaBMQpNWNh57A7RfTansaq8LbpjZsjFhPXKYiQpKwCW15frckXVVdNrtvxP8DlKuGun6tyFBRtrE7CH077JG6rOIh2b2R6lmWrvp8V8TJuy')
if ($manifest.Length -ne 104 -or $publicKey.Length -ne 422 -or $signature.Length -ne 384) { throw 'RSA-PSS vector length mismatch' }
$manifestPath = Join-Path $OutputDirectory 'manifest.bin'
$publicKeyPath = Join-Path $OutputDirectory 'public.der'
$signaturePath = Join-Path $OutputDirectory 'manifest.sig'
[System.IO.File]::WriteAllBytes($manifestPath, $manifest)
[System.IO.File]::WriteAllBytes($publicKeyPath, $publicKey)
[System.IO.File]::WriteAllBytes($signaturePath, $signature)
$openssl = 'C:\Program Files\Git\usr\bin\openssl.exe'
& $openssl dgst -sha256 -verify $publicKeyPath -keyform DER -signature $signaturePath `
    -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:32 $manifestPath
if ($LASTEXITCODE -ne 0) { throw 'Valid RSA-PSS vector rejected' }
$manifest[8] = 2
[System.IO.File]::WriteAllBytes($manifestPath, $manifest)
& $openssl dgst -sha256 -verify $publicKeyPath -keyform DER -signature $signaturePath `
    -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:32 $manifestPath
if ($LASTEXITCODE -eq 0) { throw 'Modified manifest accepted' }
Write-Output 'update_signature_vector_test: valid vector accepted; modified manifest rejected'
