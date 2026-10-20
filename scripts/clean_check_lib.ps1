$S = [char]0xA7
$CcLicenses = 7
$CcBin = @('assetc.exe', 'editor_shell.exe', 'vcruntime140.dll', 'wgpu_native.dll')
$CcTar = 'tar.exe'
$CcUtf8 = New-Object System.Text.UTF8Encoding $false

function Get-CcSize([string]$File) {
    if (Test-Path -LiteralPath $File -PathType Leaf) { (Get-Item -LiteralPath $File).Length } else { 0 }
}

function Format-CcRc([int]$Rc) {
    if ($Rc -eq -1073741515) { return "$Rc (0xC0000135 - a DLL is missing, screenshot the dialog)" }
    if ($Rc -lt 0) { return ('{0} (0x{0:X8})' -f $Rc) }
    "$Rc"
}

function Test-CcSums([string]$Sums, [string[]]$Files) {
    if (-not (Test-Path -LiteralPath $Sums -PathType Leaf)) {
        return "FAIL  ${S}15 no SHA256SUMS next to the package - copy it from release/<version>/"
    }
    $rows = @(Get-Content -LiteralPath $Sums)
    foreach ($f in $Files) {
        $name = Split-Path -Leaf $f
        $want = ''
        foreach ($r in $rows) {
            $p = $r.TrimEnd("`r") -split ' +', 2
            if ($p.Count -eq 2 -and ($p[1] -ceq $name -or $p[1] -ceq "*$name")) { $want = $p[0].ToLower() }
        }
        if (-not $want) { "FAIL  ${S}15 $name is not in SHA256SUMS"; continue }
        $got = (Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash.ToLower()
        if ($got -ceq $want) { "ok    ${S}15 sha256 $name $got" }
        else { "FAIL  ${S}15 sha256 $name is $got, SHA256SUMS says $want" }
    }
}

function Get-CcTree([string]$Dir) {
    $root = (Resolve-Path -LiteralPath $Dir).ProviderPath
    @(Get-ChildItem -LiteralPath $root -Recurse -Force | Sort-Object FullName | ForEach-Object {
            $rel = $_.FullName.Substring($root.Length)
            if ($_.PSIsContainer) { "$rel\" } else { "$rel $((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)" }
        })
}

function Test-CcUnpack([string]$Tgz, [string]$Work) {
    foreach ($d in 'a', 'b') {
        $to = Join-Path $Work $d
        New-Item -ItemType Directory -Force -Path $to | Out-Null
        $out = @(& $CcTar -xzf $Tgz -C $to 2>&1)
        if ($LASTEXITCODE) { return "FAIL  ${S}15 tar -xzf into ${d}: $($out | Select-Object -Last 1)" }
    }
    $ta = @(Get-CcTree (Join-Path $Work 'a'))
    $tb = @(Get-CcTree (Join-Path $Work 'b'))
    if (($ta -join "`n") -cne ($tb -join "`n")) {
        $i = 0
        while ($i -lt $ta.Count -and $i -lt $tb.Count -and $ta[$i] -ceq $tb[$i]) { $i++ }
        return "FAIL  ${S}15 two unpacks differ: a has '$($ta[$i])', b has '$($tb[$i])'"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Work 'a\like-nes') -PathType Container)) {
        return "FAIL  ${S}15 the archive holds no like-nes/ folder"
    }
    "ok    ${S}15 unpacked twice, the two trees agree file by file ($($ta.Count) entries)"
}

function Test-CcVersion([string]$File, [string]$Version, [string]$Triple) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { return "FAIL  ${S}15 no like-nes/version.txt" }
    $l = @(Get-Content -LiteralPath $File)
    $l | ForEach-Object { "      $_" }
    if ($l.Count -ne 3) { "FAIL  ${S}15 version.txt has $($l.Count) lines, not 3" }
    elseif ($l -match 'unknown') { "FAIL  ${S}15 version.txt says unknown" }
    elseif ($l[0] -cne "like-nes engine $Version" -or $l[2] -cne "target $Triple") {
        "FAIL  ${S}15 version.txt does not name $Version and target $Triple of the archive name"
    }
    elseif ($l[1] -cnotmatch '^commit .') { "FAIL  ${S}15 the second line of version.txt names no commit" }
    else { "ok    ${S}15 version.txt names $Version, $($l[1]) and target $Triple" }
}

function Test-CcLicenses([string]$Dir) {
    $f = @(Get-ChildItem -LiteralPath $Dir -File -Recurse -ErrorAction SilentlyContinue)
    $empty = @($f | Where-Object { $_.Length -eq 0 } | ForEach-Object { $_.Name })
    if ($f.Count -eq $CcLicenses -and $empty.Count -eq 0) { return "ok    ${S}15 licenses/ holds $($f.Count) files, none empty" }
    "FAIL  ${S}15 licenses/ holds $($f.Count) files, not $CcLicenses$(if ($empty.Count) { ' - empty: ' + ($empty -join ' ') })"
}

function Test-CcBin([string]$Dir, [string]$Where) {
    $have = @(Get-ChildItem -LiteralPath $Dir -File -ErrorAction SilentlyContinue | ForEach-Object { $_.Name.ToLower() } | Sort-Object) -join ' '
    $want = @($CcBin | Sort-Object) -join ' '
    if ($have -ceq $want) { return "ok    ${S}16 $Where holds the four files: $want" }
    "FAIL  ${S}16 $Where holds [$have], not [$want]"
}

function Test-CcVcruntime([string[]]$Found) {
    if (-not $Found) { return "ok    ${S}16 where vcruntime140.dll finds nothing - no Redistributable on this box" }
    "note  ${S}16 vcruntime140.dll is installed ($($Found -join ', ')) - this box cannot answer ${S}16, the package starts here with or without its own copy"
}

function Test-CcPng([string]$File) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { return $false }
    $b = [IO.File]::ReadAllBytes($File)
    $b.Length -ge 8 -and ((@($b[0..7]) | ForEach-Object { '{0:x2}' -f $_ }) -join '') -ceq '89504e470d0a1a0a'
}

function Test-CcAssetc([int]$Rc, [string]$Log, [string]$Bundle) {
    Get-Content -LiteralPath $Log -TotalCount 3 -ErrorAction SilentlyContinue | ForEach-Object { "      $_" }
    $size = Get-CcSize $Bundle
    if ($Rc -eq 0 -and $size -gt 0) { return "ok    ${S}15 assetc --synthetic exited 0, the bundle is $size bytes" }
    "FAIL  ${S}15 assetc --synthetic exited $(Format-CcRc $Rc)$(if (-not $size) { ', no bundle' })"
}

function Test-CcGate6([string]$What, [string]$Gate, [int]$Rc, [string]$Png, [string]$Log) {
    $isPng = Test-CcPng $Png
    if ($Rc -eq 0 -and $isPng) {
        return "ok    $S$Gate ${What}: --gate6 exited 0, $(Split-Path -Leaf $Png) is $(Get-CcSize $Png) bytes"
    }
    Get-Content -LiteralPath $Log -Tail 5 -ErrorAction SilentlyContinue | ForEach-Object { "      $_" }
    "FAIL  $S$Gate ${What}: --gate6 exited $(Format-CcRc $Rc)$(if (-not $isPng) { ', no PNG' })"
}

function Test-CcInstall([int]$Rc, [string]$Log) {
    if ($Rc -eq 0) { return "ok    ${S}16 msiexec /i /qn exited 0 - nobody was there to answer a UAC prompt" }
    $e = @(Select-String -LiteralPath $Log -Pattern 'Error \d{4}\.' -ErrorAction SilentlyContinue |
            Select-Object -First 3 | ForEach-Object { $_.Line.Trim() })
    $e | ForEach-Object { "      $_" }
    if ($e -match 'Error 1925') {
        return "FAIL  ${S}16 msiexec /i /qn exited $Rc - the package asks for elevation (Error 1925), see InstallScope in packaging/like-nes.wxs.in"
    }
    "FAIL  ${S}16 msiexec /i /qn exited $Rc"
}

function Test-CcRemoved([int]$Rc, [string]$Dir) {
    if ($Rc -ne 0) { return "FAIL  ${S}16 msiexec /x /qn exited $Rc" }
    if (Test-Path -LiteralPath $Dir) { return "FAIL  ${S}16 msiexec /x /qn exited 0, $Dir is left behind" }
    "ok    ${S}16 msiexec /x /qn exited 0, $Dir is gone"
}

function Get-CcHomes {
    $h = "$env:USERPROFILE".TrimEnd('\', '/')
    if (-not $h) { return @() }
    $r = @($h)
    try {
        $short = (New-Object -ComObject Scripting.FileSystemObject).GetFolder($h).ShortPath.TrimEnd('\')
        if ($short -and $short -ne $h) { $r += $short }
    }
    catch { }
    $r
}

function Hide-CcHome([string]$Line, [string[]]$Homes) {
    foreach ($h in $Homes) {
        if (-not $h) { continue }
        $p = ([regex]::Escape($h)) -replace '\\\\', '[\\/]'
        $Line = [regex]::Replace($Line, $p + '(?![A-Za-z0-9_-])', '~', 'IgnoreCase')
    }
    $Line
}

function Invoke-CcExe([string]$Exe, [string[]]$ArgList, [string]$Log) {
    $q = @($ArgList | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
    $o = @{ FilePath = $Exe; ArgumentList = $q; PassThru = $true; NoNewWindow = $true }
    if ($Log) { $o.RedirectStandardOutput = $Log; $o.RedirectStandardError = "$Log.err"; $o.WorkingDirectory = Split-Path -Parent $Log }
    try { $p = Start-Process @o } catch { if ($Log) { Set-Content -LiteralPath $Log -Value "$_" }; return -1 }
    $null = $p.Handle
    $p.WaitForExit()
    if ($Log) { Set-Content -LiteralPath $Log -Encoding UTF8 -Value @(Get-Content -LiteralPath $Log, "$Log.err" -ErrorAction SilentlyContinue) }
    $p.ExitCode
}
