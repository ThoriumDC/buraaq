# Prove the product compiler rebuilds itself.
# Stage0 is a previous guest binary or clang-linked boot/stage0.ll.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$GuestSrc = Join-Path $Root "compiler-buraaq\src\main.bq"
$Selftest = Join-Path $Root "compiler-buraaq\selftest\main.bq"
$Rt = Join-Path $Root "stdlib\runtime\buraaq_rt.c"
$Std = Join-Path $Root "stdlib\runtime\buraaq_std.c"
$Want = "413489"

function Find-Clang {
    if ($env:BURAAQ_CLANG -and (Test-Path $env:BURAAQ_CLANG)) { return $env:BURAAQ_CLANG }
    $sidecar = Join-Path $env:LOCALAPPDATA "buraaq\llvm\bin\clang.exe"
    if (Test-Path $sidecar) { return $sidecar }
    $stock = "C:\Program Files\LLVM\bin\clang.exe"
    if (Test-Path $stock) { return $stock }
    $cmd = Get-Command clang -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) { return $cmd.Source }
    return "clang"
}

function Link-Guest($ir, $exe) {
    $libs = @("-lwininet", "-ladvapi32", "-lws2_32")
    $lld = @()
    $clangDir = Split-Path $Clang
    $lldLink = Join-Path $clangDir "lld-link.exe"
    if (-not (Test-Path $lldLink)) { $lldLink = "C:\Program Files\LLVM\bin\lld-link.exe" }
    if (Test-Path $lldLink) { $lld = @("-fuse-ld=lld") }
    & $Clang -Wno-override-module -Wno-deprecated-declarations -O0 @lld -o $exe $ir $Rt $Std @libs
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { throw "clang link of $exe failed" }
}

function Probe-Version($exe) {
    $out = Join-Path $env:TEMP "bq-stage0-ver.out"
    $err = Join-Path $env:TEMP "bq-stage0-ver.err"
    cmd /c "`"$exe`" --version > `"$out`" 2> `"$err`""
    $v = ""
    if (Test-Path $out) { $v = Get-Content -Raw $out -ErrorAction SilentlyContinue }
    return $v
}

function Find-Stage0 {
    $selfhosted = @(
        (Join-Path $Root "compiler-buraaq\target\debug\buraaq-compiler.exe"),
        (Join-Path $Root "dist\buraaq.exe")
    )
    foreach ($c in $selfhosted) {
        if (Test-Path $c) {
            $v = Probe-Version $c
            if ($v -match "sysroot: MISSING") { continue }
            if ($v -match "self-hosted") { return $c }
        }
    }
    $ir = Join-Path $Root "compiler-buraaq\boot\stage0.ll"
    if (Test-Path $ir) {
        $exe = Join-Path $env:TEMP "bq-stage0-seed.exe"
        Link-Guest $ir $exe
        return $exe
    }
    throw "selfhost-test needs a self-hosted Buraaq compiler or compiler-buraaq/boot/stage0.ll."
}

function Hide-Rustc {
    $clang = Find-Clang
    $clangDir = Split-Path $clang
    $llvm = "C:\Program Files\LLVM\bin"
    $parts = @()
    if ($clangDir) { $parts += $clangDir }
    if (Test-Path $llvm) { $parts += $llvm }
    $kept = $env:PATH -split ';' | Where-Object { $_ -and $_ -notmatch '(?i)(\\cargo\\|\\rustc\\|\\rustup\\)' }
    $env:PATH = ($parts + $kept) -join ';'
    foreach ($name in @("CARGO", "RUSTC", "CARGO_HOME", "RUSTUP_HOME")) {
        Remove-Item "Env:$name" -ErrorAction SilentlyContinue
    }
}

function Invoke-Native($exe, $argList, $outFile, $errFile) {
    $argStr = ($argList | ForEach-Object { '"' + ($_ -replace '"','') + '"' }) -join ' '
    cmd /c "`"$exe`" $argStr > `"$outFile`" 2> `"$errFile`""
    return $LASTEXITCODE
}

function Assert-Selftest($exe, $label) {
    $runOut = Join-Path $Work "selftest-$label.exe"
    $outFile = Join-Path $Work "selftest-$label.out"
    $errFile = Join-Path $Work "selftest-$label.err"
    $env:BURAAQ_RUN_OUT = $runOut
    $code = Invoke-Native $exe @("test", $Selftest) $outFile $errFile
    $printed = ""
    if (Test-Path $outFile) { $printed = Get-Content -Raw $outFile -ErrorAction SilentlyContinue }
    $err = ""
    if (Test-Path $errFile) { $err = Get-Content -Raw $errFile -ErrorAction SilentlyContinue }
    if ($code -ne 0) { throw "$label test failed code=$code err=$err out=$printed" }
    $got = ($printed | ForEach-Object { $_.Trim() })
    if ($got -ne $Want) { throw "$label stdout want $Want got '$got'" }
    Write-Host "PASS $label selftest -> $Want"
}

Hide-Rustc
$Clang = Find-Clang
$Work = Join-Path $env:TEMP "bq-selfhost-test"
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$Stage0 = Find-Stage0

$Ir = Join-Path $Work "guest.ll"
$Err = Join-Path $Work "guest.err"
$Product = Join-Path $Work "product.exe"

$code = Invoke-Native $Stage0 @("llvm", $GuestSrc) $Ir $Err
if ($code -ne 0) {
    throw "stage0 llvm of the guest failed: $(Get-Content -Raw $Err -ErrorAction SilentlyContinue)"
}
Link-Guest $Ir $Product

Assert-Selftest $Product "product"

$Ir2 = Join-Path $Work "guest2.ll"
$Err2 = Join-Path $Work "guest2.err"
$Stage1 = Join-Path $Work "stage1.exe"
$code = Invoke-Native $Product @("llvm", $GuestSrc) $Ir2 $Err2
if ($code -ne 0) {
    throw "product llvm of the guest failed: $(Get-Content -Raw $Err2 -ErrorAction SilentlyContinue)"
}
Link-Guest $Ir2 $Stage1
Assert-Selftest $Stage1 "stage1"

$boot = Join-Path $Root "compiler-buraaq\boot"
New-Item -ItemType Directory -Force -Path $boot | Out-Null
Copy-Item -Force $Ir2 (Join-Path $boot "stage0.ll")

Write-Host "selfhost-test: guest rebuilt itself and passed selftest."
