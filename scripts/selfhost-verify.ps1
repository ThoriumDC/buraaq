# Prove the product CLI.
# Seed is a previous guest binary or clang-linked boot/stage0.ll.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Rt = Join-Path $Root "stdlib\runtime\buraaq_rt.c"
$Std = Join-Path $Root "stdlib\runtime\buraaq_std.c"
$Golden = Join-Path $Root "compiler-buraaq\golden\sample.bq"
$Selftest = Join-Path $Root "compiler-buraaq\selftest\main.bq"

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

function Dedupe-LlDeclares([string]$path) {
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($line in [System.IO.File]::ReadLines($path)) {
        if ($line.StartsWith("declare ")) {
            if (-not $seen.Add($line)) { continue }
        }
        [void]$out.Add($line)
    }
    [System.IO.File]::WriteAllLines($path, $out)
}

function Link-Guest($ir, $exe) {
    if ($ir -like "*.ll") { Dedupe-LlDeclares $ir }
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
    $out = Join-Path $env:TEMP "bq-verify-ver.out"
    $err = Join-Path $env:TEMP "bq-verify-ver.err"
    cmd /c "`"$exe`" --version > `"$out`" 2> `"$err`""
    $v = ""
    if (Test-Path $out) { $v = Get-Content -Raw $out -ErrorAction SilentlyContinue }
    return $v
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

function Find-Compiler {
    $selfhosted = @(
        (Join-Path $Root "dist\buraaq.exe"),
        (Join-Path $Root "compiler-buraaq\target\debug\buraaq-compiler.exe")
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
        $exe = Join-Path $env:TEMP "bq-verify-stage0.exe"
        Link-Guest $ir $exe
        return $exe
    }
    throw "selfhost-verify needs a self-hosted Buraaq compiler or compiler-buraaq/boot/stage0.ll."
}

function Plant-Sysroot($exe) {
    $here = Split-Path $exe
    $rt = Join-Path $here "sysroot\runtime\buraaq_rt.c"
    if (Test-Path $rt) { return }
    $sys = Join-Path $here "sysroot"
    New-Item -ItemType Directory -Force -Path $sys | Out-Null
    $runtime = Join-Path $sys "runtime"
    if (Test-Path $runtime) { Remove-Item -Recurse -Force $runtime }
    Copy-Item -Recurse -Force (Join-Path $Root "stdlib\runtime") $runtime
    $srcIn = Join-Path $Root "stdlib\src"
    if (Test-Path $srcIn) {
        $srcOut = Join-Path $sys "src"
        if (Test-Path $srcOut) { Remove-Item -Recurse -Force $srcOut }
        Copy-Item -Recurse -Force $srcIn $srcOut
    }
}

function Assert-Printed($got, $want, $label) {
    $trim = ($got | ForEach-Object { $_.Trim() })
    if ($trim -ne $want) { throw "$label want '$want' got '$trim'" }
    Write-Host "PASS $label -> $want"
}

Hide-Rustc
$Clang = Find-Clang
$Work = Join-Path $env:TEMP "bq-selfhost-verify"
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$Bq = Find-Compiler
Plant-Sysroot $Bq

$verFile = Join-Path $Work "ver.out"
$verErr = Join-Path $Work "ver.err"
$code = Invoke-Native $Bq @("--version") $verFile $verErr
$ver = ""
if (Test-Path $verFile) { $ver = Get-Content -Raw $verFile -ErrorAction SilentlyContinue }
if ($code -ne 0 -or $ver -notmatch "self-hosted") {
    throw "compiler --version must contain self-hosted, got '$ver'"
}
Write-Host "PASS version self-hosted"

$docFile = Join-Path $Work "doctor.out"
$docErr = Join-Path $Work "doctor.err"
$code = Invoke-Native $Bq @("doctor") $docFile $docErr
$doc = ""
if (Test-Path $docFile) { $doc = Get-Content -Raw $docFile -ErrorAction SilentlyContinue }
if ($code -ne 0) { throw "doctor failed: $doc $(Get-Content -Raw $docErr -ErrorAction SilentlyContinue)" }
Write-Host "PASS doctor"

$runOut = Join-Path $Work "golden.exe"
$env:BURAAQ_RUN_OUT = $runOut
$gOut = Join-Path $Work "golden.out"
$gErr = Join-Path $Work "golden.err"
$code = Invoke-Native $Bq @("run", $Golden) $gOut $gErr
$printed = ""
if (Test-Path $gOut) { $printed = Get-Content -Raw $gOut -ErrorAction SilentlyContinue }
if ($code -ne 0) { throw "golden run failed: $(Get-Content -Raw $gErr -ErrorAction SilentlyContinue)" }
Assert-Printed $printed "5" "golden/sample.bq"

$stOut = Join-Path $Work "selftest.exe"
$env:BURAAQ_RUN_OUT = $stOut
$sOut = Join-Path $Work "selftest.out"
$sErr = Join-Path $Work "selftest.err"
$code = Invoke-Native $Bq @("test", $Selftest) $sOut $sErr
$printed = ""
if (Test-Path $sOut) { $printed = Get-Content -Raw $sOut -ErrorAction SilentlyContinue }
if ($code -ne 0) { throw "selftest failed: $(Get-Content -Raw $sErr -ErrorAction SilentlyContinue)" }
Assert-Printed $printed "413489" "selftest"

$proj = Join-Path $Work "hello"
if (Test-Path $proj) { Remove-Item -Recurse -Force $proj }
$nOut = Join-Path $Work "new.out"
$nErr = Join-Path $Work "new.err"
$code = Invoke-Native $Bq @("-C", $Work, "new", "hello", "--cli") $nOut $nErr
if ($code -ne 0) { throw "new --cli failed: $(Get-Content -Raw $nErr -ErrorAction SilentlyContinue)" }
$entry = Join-Path $proj "src\main.bq"
if (-not (Test-Path $entry)) { throw "new --cli did not write src/main.bq" }

$helloOut = Join-Path $Work "hello-run.exe"
$env:BURAAQ_RUN_OUT = $helloOut
$hOut = Join-Path $Work "hello.out"
$hErr = Join-Path $Work "hello.err"
$code = Invoke-Native $Bq @("-C", $proj, "run") $hOut $hErr
if ($code -ne 0) { throw "project run failed: $(Get-Content -Raw $hErr -ErrorAction SilentlyContinue) $(Get-Content -Raw $hOut -ErrorAction SilentlyContinue)" }
Write-Host "PASS new --cli then run"

$stress = @(
    @{ Name = "add"; Want = "5"; Src = "fn add(a: int, b: int) -> int { a + b }`nfn main() { print(add(2, 3)) }`n" },
    @{ Name = "loop"; Want = "10"; Src = "fn main() {`n    mut n = 0`n    mut k = 0`n    while k < 5 {`n        n = n + k`n        k = k + 1`n    }`n    print(n)`n}`n" },
    @{ Name = "match"; Want = "2"; Src = "enum Color {`n    Red`n    Blue`n}`nfn main() {`n    c = Color.Blue`n    match c {`n        Color.Red => {`n            print(1)`n        }`n        Color.Blue => {`n            print(2)`n        }`n    }`n}`n" },
    @{ Name = "defer"; Want = "19"; Src = "fn main() {`n    defer print(9)`n    print(1)`n}`n" },
    @{ Name = "spawn"; Want = "24"; Src = "fn tick(n: int) -> int { n + 1 }`nfn main() {`n    handle = spawn {`n        print(2)`n    }`n    handle.join()`n    print(tick(3))`n}`n" }
)
foreach ($case in $stress) {
    $src = Join-Path $Work ($case.Name + ".bq")
    Set-Content -Encoding ascii -Path $src -Value $case.Src
    $exe = Join-Path $Work ($case.Name + ".exe")
    $env:BURAAQ_RUN_OUT = $exe
    $o = Join-Path $Work ($case.Name + ".out")
    $e = Join-Path $Work ($case.Name + ".err")
    $code = Invoke-Native $Bq @("run", $src) $o $e
    $printed = ""
    if (Test-Path $o) { $printed = Get-Content -Raw $o -ErrorAction SilentlyContinue }
    if ($code -ne 0) { throw "stress $($case.Name) failed: $(Get-Content -Raw $e -ErrorAction SilentlyContinue) $printed" }
    Assert-Printed $printed $case.Want ("stress " + $case.Name)
}

$stressFile = Join-Path $Root "examples\release-gate\print_stress.bq"
$stOut2 = Join-Path $Work "print-stress.exe"
$env:BURAAQ_RUN_OUT = $stOut2
$pOut = Join-Path $Work "print-stress.out"
$pErr = Join-Path $Work "print-stress.err"
$code = Invoke-Native $Bq @("run", $stressFile) $pOut $pErr
$printed = ""
if (Test-Path $pOut) { $printed = Get-Content -Raw $pOut -ErrorAction SilentlyContinue }
if ($code -ne 0) { throw "print_stress failed: $(Get-Content -Raw $pErr -ErrorAction SilentlyContinue) $printed" }
$got = ($printed -replace "`r`n", "`n")
foreach ($needle in @("hello", "1.5", "mix 7 2.5 false", "abcd", "0.5")) {
    if ($got -notmatch [regex]::Escape($needle)) {
        throw "print_stress missing '$needle' in '$got'"
    }
}
Write-Host "PASS print_stress"

Write-Host "selfhost-verify: product CLI, golden, selftest, and guest stress passed."
