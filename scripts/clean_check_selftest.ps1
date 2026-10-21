$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'clean_check.ps1')
$Pass = 0
$Fail = 0
$T = Join-Path ([IO.Path]::GetTempPath()) ('clean-check-selftest.' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Force -Path $T | Out-Null

function Check([string]$Name, [string]$Want, [string]$Needle, [scriptblock]$Block) {
    $out = @(& $Block 2>&1 | ForEach-Object { "$_" })
    $got = if (@($out | Where-Object { $_ -clike 'FAIL*' }).Count) { 'fail' } else { 'pass' }
    if ($got -eq $Want -and ($out -join "`n").Contains($Needle)) {
        $script:Pass++
        Write-Host "clean-check-ps1-selftest: ok   $Name"
        return
    }
    $script:Fail++
    Write-Host "clean-check-ps1-selftest: FAIL $Name - want $Want with '$Needle', got $got"
    $out | ForEach-Object { Write-Host "    $_" }
}

function Put([string]$Path, [string]$Text) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path, $Text, $CcUtf8)
}

$Fx = Join-Path $T 'fixture.exe'
Add-Type -OutputType ConsoleApplication -OutputAssembly $Fx -TypeDefinition @'
using System;
using System.IO;
public static class Fixture {
    public static int Main(string[] a) {
        if (a.Length != 2) return 9;
        if (a[0] == "--synthetic") { File.WriteAllText(a[1], "bundle"); Console.WriteLine("assetc: baked " + a[1]); return 0; }
        if (a[0] == "--gate6") { File.WriteAllText("imgui.ini", "x"); File.WriteAllBytes(a[1], new byte[] { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a }); return 0; }
        if (a[0] == "--exit") { Console.Error.WriteLine("crash in free"); return int.Parse(a[1]); }
        return 9;
    }
}
'@

function Stage([string]$Dir, [string]$Triple) {
    $s = Join-Path $Dir 'like-nes'
    New-Item -ItemType Directory -Force -Path (Join-Path $s 'bin') | Out-Null
    Copy-Item $Fx (Join-Path $s 'bin\assetc.exe')
    Copy-Item $Fx (Join-Path $s 'bin\editor_shell.exe')
    Put (Join-Path $s 'bin\wgpu_native.dll') 'dll'
    Put (Join-Path $s 'bin\vcruntime140.dll') 'dll'
    Put (Join-Path $s 'version.txt') "like-nes engine v9.1.0`ncommit abc123`ntarget $Triple`n"
    1..7 | ForEach-Object { Put (Join-Path $s "licenses\L$_") "license $_" }
}

Stage "$T\st" 'windows-x86_64'
& tar.exe -czf "$T\p.tar.gz" -C "$T\st" like-nes
$h = (Get-FileHash -LiteralPath "$T\p.tar.gz" -Algorithm SHA256).Hash.ToLower()
Put "$T\SUMS" "$h  p.tar.gz`r`n"
Put "$T\SUMS-bad" "0000  p.tar.gz`n"
Check 'a matching sum passes, CRLF or not' pass "ok    ${S}15 sha256 p.tar.gz $h" { Test-CcSums "$T\SUMS" @("$T\p.tar.gz") }
Check 'a wrong sum fails' fail 'SHA256SUMS says 0000' { Test-CcSums "$T\SUMS-bad" @("$T\p.tar.gz") }
Check 'a package missing from the sums fails' fail 'x.msi is not in SHA256SUMS' { Test-CcSums "$T\SUMS" @("$T\p.tar.gz", "$T\x.msi") }
Check 'no SHA256SUMS fails' fail 'no SHA256SUMS' { Test-CcSums "$T\none" @("$T\p.tar.gz") }

function Unpack([string]$Tgz) { Remove-Item -Recurse -Force "$T\u" -ErrorAction SilentlyContinue; Test-CcUnpack $Tgz "$T\u" }
Check 'two unpacks agree' pass 'the two trees agree file by file' { Unpack "$T\p.tar.gz" }
$CcTar = { & tar.exe @args; if ("$args".Contains("$T\u\b")) { Put "$T\u\b\like-nes\extra" 'x' } }
Check 'two unpacks that differ fail' fail 'b has ''\like-nes\extra ' { Unpack "$T\p.tar.gz" }
$CcTar = 'tar.exe'
& tar.exe -czf "$T\flat.tar.gz" -C "$T\st\like-nes" version.txt
Check 'an archive without like-nes/ fails' fail 'no like-nes/ folder' { Unpack "$T\flat.tar.gz" }
Check 'a broken archive fails' fail 'tar -xzf into a' { Unpack "$T\SUMS" }

function Version([string]$Text) { Put "$T\v.txt" $Text; Test-CcVersion "$T\v.txt" 'v9.1.0' 'windows-x86_64' }
Check 'version.txt of the archive passes' pass 'names v9.1.0, commit abc123 and target windows-x86_64' {
    Test-CcVersion "$T\st\like-nes\version.txt" 'v9.1.0' 'windows-x86_64' }
Check 'unknown fails' fail 'says unknown' { Version "like-nes engine v9.1.0`ncommit unknown`ntarget windows-x86_64`n" }
Check 'two lines fail' fail 'has 2 lines' { Version "like-nes engine v9.1.0`ntarget windows-x86_64`n" }
Check 'another triple fails' fail 'does not name v9.1.0 and target windows-x86_64' {
    Version "like-nes engine v9.1.0`ncommit a`ntarget linux-x86_64`n" }
Check 'no commit fails' fail 'names no commit' { Version "like-nes engine v9.1.0`nbuilt a`ntarget windows-x86_64`n" }

$L = "$T\st\like-nes\licenses"
Check 'seven licenses pass' pass 'holds 7 files, none empty' { Test-CcLicenses $L }
Move-Item "$L\L7" "$T\L7"
Check 'six licenses fail' fail 'holds 6 files, not 7' { Test-CcLicenses $L }
Put "$L\L7" ''
Check 'an empty license fails' fail 'empty: L7' { Test-CcLicenses $L }
Remove-Item "$L\L7"
Move-Item "$T\L7" "$L\L7"

$B = "$T\st\like-nes\bin"
Check 'four files in bin pass' pass 'holds the four files: assetc.exe editor_shell.exe vcruntime140.dll wgpu_native.dll' { Test-CcBin $B 'bin' }
Move-Item "$B\vcruntime140.dll" "$T\vc.dll"
Check 'bin without vcruntime140.dll fails' fail 'holds [assetc.exe editor_shell.exe wgpu_native.dll]' { Test-CcBin $B 'bin' }
Move-Item "$T\vc.dll" "$B\vcruntime140.dll"
Put "$B\extra.dll" 'x'
Check 'a fifth file in bin fails' fail 'extra.dll' { Test-CcBin $B 'bin' }
Remove-Item "$B\extra.dll"
Check 'no Redistributable passes' pass 'finds nothing' { Test-CcVcruntime @() }
Check 'an installed Redistributable is a note' pass 'cannot answer' { Test-CcVcruntime @('C:\Windows\System32\vcruntime140.dll') }

Put "$T\a.out" 'assetc: baked'
Put "$T\bundle" 'bundle'
Check 'assetc that bakes passes' pass 'the bundle is 6 bytes' { Test-CcAssetc 0 "$T\a.out" "$T\bundle" }
Check 'assetc without a bundle fails' fail 'exited 0, no bundle' { Test-CcAssetc 0 "$T\a.out" "$T\none" }
Check 'a missing DLL is named' fail '0xC0000135 - a DLL is missing' { Test-CcAssetc -1073741515 "$T\a.out" "$T\none" }
Check 'a heap crash is named in hex' fail '(0xC0000374)' { Test-CcAssetc -1073740940 "$T\a.out" "$T\bundle" }

New-Item -ItemType Directory -Force -Path "$T\a b" | Out-Null
$rc = Invoke-CcExe $Fx @('--gate6', "$T\a b\g.png") "$T\a b\g.out"
Check 'a path with a space reaches the process' pass 'g.png is 8 bytes' { Test-CcGate6 x 16 $rc "$T\a b\g.png" "$T\a b\g.out" }
Check 'the program runs in the folder of its log' pass '' {
    if (-not (Test-Path -LiteralPath "$T\a b\imgui.ini")) { 'FAIL  no imgui.ini next to the log' }
    if (Test-Path -LiteralPath (Join-Path $PWD 'imgui.ini')) { 'FAIL  imgui.ini in the current folder' }
}
$rc = Invoke-CcExe $Fx @('--exit', '3') "$T\g.out"
Check 'the exit code and stderr come back' fail 'crash in free' { Test-CcGate6 x 16 $rc "$T\none.png" "$T\g.out" }
Check 'a crash names its code' fail '--gate6 exited 3, no PNG' { Test-CcGate6 x 16 $rc "$T\none.png" "$T\g.out" }
Check 'a PNG with a crash fails' fail '--gate6 exited 3' { Test-CcGate6 x 16 3 "$T\a b\g.png" "$T\g.out" }
Check 'a text file is no PNG' fail 'exited 0, no PNG' { Test-CcGate6 x 16 0 "$T\a.out" "$T\g.out" }
Check 'a missing program fails' fail 'exited -1' { Test-CcGate6 x 16 (Invoke-CcExe "$T\none.exe" @('--gate6', "$T\n.png") "$T\n.out") "$T\n.png" "$T\n.out" }

Put "$T\msi.log" "MSI (s) (A8:1C) Product: like-nes engine -- Error 1925. You do not have sufficient privileges`n"
Check 'a silent install that exits 0 passes' pass 'exited 0' { Test-CcInstall 0 "$T\msi.log" }
Check 'Error 1925 is named' fail 'asks for elevation (Error 1925)' { Test-CcInstall 1603 "$T\msi.log" }
Check 'another code fails plain' fail 'exited 1618' { Test-CcInstall 1618 "$T\none.log" }
Check 'a removed folder passes' pass 'is gone' { Test-CcRemoved 0 "$T\gone" }
Check 'a left folder fails' fail 'is left behind' { Test-CcRemoved 0 "$T\st" }
Check 'a failed uninstall fails' fail 'msiexec /x /qn exited 1605' { Test-CcRemoved 1605 "$T\gone" }

Check 'the home folder is masked, any case and slash' pass '~\x and ~ and ''~'' and C:\Users\alx\y and ~/z, ~. ~;' {
    Hide-CcHome "C:\Users\al\x and c:\users\AL and 'C:\Users\al' and C:\Users\alx\y and C:/Users/al/z, C:\Users\al. C:\Users\al;" @('C:\Users\al') }
Check 'no home masks nothing' pass 'C:\Users\al\x' { Hide-CcHome 'C:\Users\al\x' @() }

. (Join-Path $PSScriptRoot 'clean_check_e2e.ps1')

Remove-Item -Recurse -Force $T -ErrorAction SilentlyContinue
Write-Host "clean-check-ps1-selftest: $Pass ok, $Fail failed"
if ($Fail -or -not $Pass) { exit 1 }
exit 0
