param([string]$Tgz = '')

$ErrorActionPreference = 'Continue'
$CcLib = Join-Path $PSScriptRoot 'clean_check_lib.ps1'
if (-not (Test-Path -LiteralPath $CcLib -PathType Leaf)) {
    [Console]::Error.WriteLine('clean-check: copy clean_check_lib.ps1 next to clean_check.ps1')
    exit 2
}
. $CcLib

function Write-CcLine([string]$Line) {
    $m = Hide-CcHome $Line $script:CcHomes
    Write-Host $m
    [IO.File]::AppendAllText($script:CcReport, "$m`n", $CcUtf8)
}

function Invoke-CcStep([scriptblock]$Block) {
    foreach ($l in @(& $Block)) {
        Write-CcLine "$l"
        if ("$l" -clike 'FAIL*') { $script:CcRc = 1 }
    }
}

function Get-CcTriple {
    $a = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }
    if ($a -eq 'AMD64') { 'windows-x86_64' } else { "windows-$("$a".ToLower())" }
}

function Invoke-CcMsi([string[]]$ArgList) { Invoke-CcExe 'msiexec.exe' $ArgList '' }

function Get-CcWhere { @(& where.exe vcruntime140.dll 2>$null | Where-Object { $_ }) }

function Invoke-CcInstaller([string]$Msi, [string]$Work, [string]$Base) {
    $dir = Join-Path $env:LOCALAPPDATA 'like-nes'
    if (Test-Path -LiteralPath $dir) {
        return "FAIL  ${S}16 $dir already exists - uninstall like-nes engine from Installed apps first"
    }
    $log = Join-Path $Work 'msi.log'
    $rc = Invoke-CcMsi @('/i', $Msi, '/qn', '/l*v', $log)
    Test-CcInstall $rc $log
    if ($rc -ne 0) { return }
    Test-CcBin (Join-Path $dir 'bin') 'the installed bin'
    $exe = Join-Path $dir 'bin\editor_shell.exe'
    $png = "$Base-msi.png"
    $grc = Invoke-CcExe $exe @('--gate6', $png) (Join-Path $Work 'msi.out')
    Test-CcGate6 'editor_shell.exe installed by the .msi' 16 $grc $png (Join-Path $Work 'msi.out')
    Test-CcRemoved (Invoke-CcMsi @('/x', $Msi, '/qn')) $dir
}

function Get-CcByHand([string]$Stem) {
    'by hand - what the script cannot see:'
    "  1. double-click $Stem.msi -> SmartScreen: unknown publisher, More info > Run anyway; then no UAC prompt"
    '  2. Start menu > like-nes engine -> the editor window opens, no VCRUNTIME140.dll dialog'
    '  3. the same .msi again, then a newer one on top -> one entry in Installed apps'
    '  4. an older .msi on top of a newer install -> refuses: A newer version of like-nes engine is already installed'
    '  5. uninstall from Installed apps -> %LOCALAPPDATA%\like-nes is gone'
    '  send: this report and the PNG files next to it, or on the dev machine: make owner-import ARGS=<report>'
}

function Invoke-CcMain([string]$Tgz) {
    $triple = Get-CcTriple
    if (-not $Tgz) {
        $c = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter "like-nes-engine-*-$triple.tar.gz" -File)
        if ($c.Count -ne 1) {
            [Console]::Error.WriteLine("clean-check: want one like-nes-engine-<version>-$triple.tar.gz next to this script")
            return 2
        }
        $Tgz = $c[0].FullName
    }
    if (-not (Test-Path -LiteralPath $Tgz -PathType Leaf)) { [Console]::Error.WriteLine("clean-check: no $Tgz"); return 2 }
    $Tgz = (Resolve-Path -LiteralPath $Tgz).ProviderPath
    $dir = Split-Path -Parent $Tgz
    $stem = (Split-Path -Leaf $Tgz) -replace '\.tar\.gz$', ''
    if ($stem -cnotmatch ('^like-nes-engine-(.+)-' + [regex]::Escape($triple) + '$')) {
        [Console]::Error.WriteLine("clean-check: $stem.tar.gz is not a like-nes-engine-<version>-$triple package")
        return 2
    }
    $version = $Matches[1]
    $stamp = (Get-Date).ToUniversalTime().ToString('yyMMdd-HHmmss')
    $base = Join-Path $dir "clean-check-windows-$stamp"
    $script:CcReport = "$base.txt"
    $script:CcRc = 0
    $script:CcHomes = @(Get-CcHomes)
    $msi = Join-Path $dir "$stem.msi"
    $work = Join-Path ([IO.Path]::GetTempPath()) ('clean-check.' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Force -Path $work | Out-Null
    try {
        $os = 'Windows'
        try { $os = (Get-CimInstance Win32_OperatingSystem).Caption } catch { }
        Write-CcLine "clean-check $stem on windows at $stamp"
        Write-CcLine "system: $os ($([Environment]::OSVersion.Version) $env:PROCESSOR_ARCHITECTURE)"
        $sums = @($Tgz) + @($msi | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
        Invoke-CcStep { Test-CcSums (Join-Path $dir 'SHA256SUMS') $sums }
        $found = @(Get-CcWhere)
        Invoke-CcStep { Test-CcVcruntime $found }
        Invoke-CcStep { Test-CcUnpack $Tgz $work }
        $tree = Join-Path $work 'a\like-nes'
        Invoke-CcStep { Test-CcVersion (Join-Path $tree 'version.txt') $version $triple }
        Invoke-CcStep { Test-CcLicenses (Join-Path $tree 'licenses') }
        Invoke-CcStep { Test-CcBin (Join-Path $tree 'bin') 'like-nes\bin of the archive' }
        $bundle = Join-Path $work 'probe.bundle'
        $arc = Invoke-CcExe (Join-Path $tree 'bin\assetc.exe') @('--synthetic', $bundle) (Join-Path $work 'assetc.out')
        Invoke-CcStep { Test-CcAssetc $arc (Join-Path $work 'assetc.out') $bundle }
        $grc = Invoke-CcExe (Join-Path $tree 'bin\editor_shell.exe') @('--gate6', "$base-tar.png") (Join-Path $work 'tar.out')
        Invoke-CcStep { Test-CcGate6 'editor_shell.exe from the archive' 16 $grc "$base-tar.png" (Join-Path $work 'tar.out') }
        if (Test-Path -LiteralPath $msi -PathType Leaf) { Invoke-CcStep { Invoke-CcInstaller $msi $work $base } }
        else { Invoke-CcStep { "FAIL  ${S}16 no $stem.msi next to the archive - copy it from release/$version/" } }
        Get-CcByHand $stem | ForEach-Object { Write-CcLine $_ }
        if ($script:CcRc) { Write-CcLine 'clean-check: FAIL' }
        elseif ($found) { Write-CcLine "clean-check: PASS, but ${S}16 stays open - the Redistributable is installed on this box" }
        else { Write-CcLine 'clean-check: PASS' }
    }
    finally { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
    Write-Host "clean-check: report $(Hide-CcHome $script:CcReport $script:CcHomes)"
    $script:CcRc
}

if ($MyInvocation.InvocationName -ne '.') { exit (Invoke-CcMain $Tgz) }
