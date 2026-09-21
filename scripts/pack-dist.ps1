# Pack the self-hosted compiler into dist\.
# Stage0 is a previous guest binary or clang-linked compiler-buraaq/boot/stage0.ll.
# The artifact written to dist\ is always the guest: Buraaq compiling Buraaq.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dist = Join-Path $Root "dist"
$GuestSrc = Join-Path $Root "compiler-buraaq\src\main.bq"
$Rt = Join-Path $Root "stdlib\runtime\buraaq_rt.c"
$Std = Join-Path $Root "stdlib\runtime\buraaq_std.c"
$Out = Join-Path $Dist "buraaq.exe"

function Find-Clang {
    if ($env:BURAAQ_CLANG -and (Test-Path $env:BURAAQ_CLANG)) { return $env:BURAAQ_CLANG }
    $sidecar = Join-Path $env:LOCALAPPDATA "buraaq\llvm\bin\clang.exe"
    if (Test-Path $sidecar) { return $sidecar }
    $stock = "C:\Program Files\LLVM\bin\clang.exe"
    if (Test-Path $stock) { return $stock }
    return "clang"
}

function Link-Guest($ir, $exe) {
    $libs = @("-lwininet", "-ladvapi32", "-lws2_32")
    & $Clang -Wno-override-module -Wno-deprecated-declarations -O2 -fuse-ld=lld -o $exe $ir $Rt $Std @libs
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { throw "clang link of $exe failed" }
}

function Probe-Version($exe) {
    $out = Join-Path $env:TEMP "bq-pack-ver.out"
    $err = Join-Path $env:TEMP "bq-pack-ver.err"
    cmd /c "`"$exe`" --version > `"$out`" 2> `"$err`""
    $v = ""
    if (Test-Path $out) { $v = Get-Content -Raw $out -ErrorAction SilentlyContinue }
    return $v
}

function Find-Stage0 {
    $selfhosted = @(
        (Join-Path $Root "compiler-buraaq\target\debug\buraaq-compiler.exe"),
        (Join-Path $Dist "buraaq.exe")
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
        $exe = Join-Path $env:TEMP "bq-pack-stage0.exe"
        Link-Guest $ir $exe
        return $exe
    }
    throw "pack-dist needs a self-hosted Buraaq compiler or compiler-buraaq/boot/stage0.ll."
}

function Copy-Sysroot {
    $sys = Join-Path $Dist "sysroot"
    New-Item -ItemType Directory -Force -Path (Join-Path $sys "runtime") | Out-Null
    $stdlib = Join-Path $Root "stdlib"
    foreach ($part in @("src", "runtime")) {
        $from = Join-Path $stdlib $part
        $to = Join-Path $sys $part
        if (Test-Path $from) {
            if (Test-Path $to) { Remove-Item -Recurse -Force $to }
            Copy-Item -Recurse $from $to
        }
    }
    Copy-Item -Force $Rt (Join-Path $sys "runtime\buraaq_rt.c")
    $pkg = Join-Path $stdlib "buraaq.pkg"
    if (Test-Path $pkg) { Copy-Item -Force $pkg (Join-Path $sys "buraaq.pkg") }
}

New-Item -ItemType Directory -Force -Path $Dist | Out-Null
$Clang = Find-Clang
$Stage0 = Find-Stage0
$Work = Join-Path $env:TEMP "bq-pack-dist"
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$Ir = Join-Path $Work "guest.ll"
$Err = Join-Path $Work "guest.err"
$Stage1 = Join-Path $Work "stage1.exe"

$code = cmd /c "`"$Stage0`" llvm `"$GuestSrc`" > `"$Ir`" 2> `"$Err`""
if ($LASTEXITCODE -ne 0) {
    throw "stage0 llvm failed: $(Get-Content -Raw $Err -ErrorAction SilentlyContinue)"
}
Link-Guest $Ir $Stage1
Copy-Item -Force $Stage1 $Out

Copy-Sysroot

$boot = Join-Path $Root "compiler-buraaq\boot"
New-Item -ItemType Directory -Force -Path $boot | Out-Null
if (Test-Path $Ir) {
    Copy-Item -Force $Ir (Join-Path $boot "stage0.ll")
}

Write-Host "Packed $Out (self-hosted guest)"
Write-Host "Users install with: .\install.ps1"
