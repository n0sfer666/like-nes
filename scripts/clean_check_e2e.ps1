$Tr = Get-CcTriple
$P = "$T\home\pkg"
New-Item -ItemType Directory -Force -Path $P, "$T\home\tmp", "$T\home\AppData\Local" | Out-Null
Stage "$T\st2" $Tr
$Tgz = "$P\like-nes-engine-v9.1.0-$Tr.tar.gz"
$Msi = "$P\like-nes-engine-v9.1.0-$Tr.msi"
& tar.exe -czf $Tgz -C "$T\st2" like-nes
Put $Msi 'msi'
Put "$P\SHA256SUMS" (@($Tgz, $Msi | ForEach-Object {
            "$((Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLower())  $(Split-Path -Leaf $_)`n" }) -join '')
$Where = @()
$MsiRc = 0

function Get-CcWhere { $Where }

function Invoke-CcMsi([string[]]$ArgList) {
    $d = Join-Path $env:LOCALAPPDATA 'like-nes'
    if ($ArgList[0] -eq '/x') { Remove-Item -Recurse -Force $d -ErrorAction SilentlyContinue; return 0 }
    if ($MsiRc) { Put $ArgList[4] "Error 1925. You do not have sufficient privileges`n"; return $MsiRc }
    New-Item -ItemType Directory -Force -Path $d | Out-Null
    Copy-Item -Recurse "$T\st2\like-nes\bin" $d
    0
}

function E2e {
    Remove-Item "$P\clean-check-*" -ErrorAction SilentlyContinue
    $rc = Invoke-CcMain $Tgz 6>$null
    Get-ChildItem -LiteralPath $P -Filter 'clean-check-*.png' | ForEach-Object { $_.Name }
    Get-Content -LiteralPath $script:CcReport
    "rc $rc"
}

$Saved = @{ USERPROFILE = $env:USERPROFILE; LOCALAPPDATA = $env:LOCALAPPDATA; TEMP = $env:TEMP; TMP = $env:TMP }
$env:USERPROFILE = "$T\home"
$env:LOCALAPPDATA = "$T\home\AppData\Local"
$env:TEMP = "$T\home\tmp"
$env:TMP = "$T\home\tmp"

Check 'the clean box run passes end to end' pass "clean-check: PASS`nrc 0" { E2e }
Check 'the report masks the home folder' pass 'assetc: baked ~\tmp\clean-check.' { E2e }
Check 'the installed bin is judged' pass 'the installed bin holds the four files' { E2e }
Check 'the installer sum is judged' pass "sha256 $(Split-Path -Leaf $Msi) " { E2e }
Check 'the uninstall is judged' pass "~\AppData\Local\like-nes is gone" { E2e }
Check 'the run leaves both frames next to the report' pass '-msi.png' { E2e }
Check 'the report carries no home folder' pass '' {
    $home2 = "$T\home".ToLower()
    $n = @(E2e | Where-Object { $_.ToLower().Contains($home2) })
    if (-not (Test-Path "$P\clean-check-*.txt")) { 'FAIL  no report' }
    if ($n.Count) { "FAIL  $($n[0])" }
}
$Where = @('C:\Windows\System32\vcruntime140.dll')
Check 'a box with the Redistributable keeps 16 open' pass 'PASS, but' { E2e }
$Where = @()
$MsiRc = 1603
Check 'an install that asks for elevation fails the run' fail 'asks for elevation (Error 1925)' { E2e }
$MsiRc = 0
New-Item -ItemType Directory -Force -Path "$T\home\AppData\Local\like-nes" | Out-Null
Check 'a previous install refuses the run' fail 'already exists' { E2e }
Remove-Item -Recurse -Force "$T\home\AppData\Local\like-nes"
Move-Item $Msi "$T\inst.msi"
Check 'a missing installer fails the run' fail "no $(Split-Path -Leaf $Msi) next to the archive" { E2e }
Move-Item "$T\inst.msi" $Msi
Check 'no package next to the script refuses a guess' pass 'rc 2' { "rc $(Invoke-CcMain '')" }
New-Item -ItemType Directory -Force -Path "$T\lone" | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'clean_check.ps1') "$T\lone"
Check 'a script without its lib refuses' pass 'copy clean_check_lib.ps1 next to clean_check.ps1' {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$T\lone\clean_check.ps1" 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 2) { "FAIL  exited $LASTEXITCODE, not 2" }
}

foreach ($k in $Saved.Keys) { Set-Item -Path "env:$k" -Value $Saved[$k] }
