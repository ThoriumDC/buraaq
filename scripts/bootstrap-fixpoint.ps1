# Drive the guest compiler through stage1 and stage2 and report where it stops.
#
#   stage0  self-hosted guest       -> ir1
#   stage1  link(ir1)               -> ir2
#   stage2  link(ir2)               -> ir3
#
# ir2 == ir3 is the bootstrap fixpoint: the compiler reproduces itself exactly.
param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$Work = "$env:TEMP\bq-fixpoint"
)

$ErrorActionPreference = "Stop"

$bq = $null
foreach ($c in @(
    (Join-Path $Root "dist\buraaq.exe"),
    (Join-Path $Root "compiler-buraaq\target\debug\buraaq-compiler.exe")
)) {
    if (Test-Path $c) { $bq = $c; break }
}
if (-not $bq) { throw "bootstrap-fixpoint needs dist\buraaq.exe or a guest binary. Run scripts/pack-dist.ps1 first." }
$clang = @(
    "$env:LOCALAPPDATA\buraaq\llvm\bin\clang.exe",
    "C:\Program Files\LLVM\bin\clang.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $clang) { $clang = "clang" }

$libs = @("-lwininet", "-ladvapi32", "-lws2_32")
$rts = @(
    (Join-Path $Root "stdlib\runtime\buraaq_rt.c"),
    (Join-Path $Root "stdlib\runtime\buraaq_std.c")
)

Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $Work | Out-Null

# The guest follows `use` itself, so it is handed one entry point and reaches the
# rest from there. src/main.bq pulls in every module, so this covers the whole
# compiler rather than just its emitter, and nothing is woven by this script.
$unit = Join-Path $Root "compiler-buraaq\src\main.bq"

Push-Location (Join-Path $Root "compiler-buraaq")
& $bq build | Out-Null
Pop-Location
$stage0 = Join-Path $Root "compiler-buraaq\target\debug\buraaq-compiler.exe"

function Emit($exe, $out) {
    cmd /c "`"$exe`" llvm `"$unit`" > `"$out`" 2>nul"
    if ($LASTEXITCODE -ne 0) { throw "$(Split-Path $exe -Leaf) exited $LASTEXITCODE compiling the unit" }
}

function Link($ir, $exe) {
    # clang writes warnings to stderr; only a missing output means failure.
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $log = & $clang -Wno-override-module -Wno-deprecated-declarations -O0 `
        -fuse-ld=lld -o $exe $ir @rts @libs 2>&1
    $ErrorActionPreference = $prev
    if (-not (Test-Path $exe)) {
        $log | Select-String "error" | ForEach-Object { Write-Host $_.Line }
        throw "link failed for $ir"
    }
}

cmd /c "`"$stage0`" llvm `"$unit`" > `"$Work\ir1.ll`" 2>nul"
Write-Host ("stage0 -> ir1   {0,6} lines" -f (Get-Content "$Work\ir1.ll").Count)

Link "$Work\ir1.ll" "$Work\stage1.exe"
Emit "$Work\stage1.exe" "$Work\ir2.ll"
Write-Host ("stage1 -> ir2   {0,6} lines" -f (Get-Content "$Work\ir2.ll").Count)

Link "$Work\ir2.ll" "$Work\stage2.exe"
Emit "$Work\stage2.exe" "$Work\ir3.ll"
Write-Host ("stage2 -> ir3   {0,6} lines" -f (Get-Content "$Work\ir3.ll").Count)

$same = (Get-FileHash "$Work\ir2.ll").Hash -eq (Get-FileHash "$Work\ir3.ll").Hash
if ($same) {
    Write-Host "FIXPOINT: ir2 == ir3" -ForegroundColor Green
    exit 0
}
Write-Host "NO FIXPOINT: ir2 != ir3" -ForegroundColor Yellow
$a = Get-Content "$Work\ir2.ll"; $b = Get-Content "$Work\ir3.ll"
$n = [Math]::Min($a.Count, $b.Count)
for ($i = 0; $i -lt $n; $i++) {
    if ($a[$i] -ne $b[$i]) {
        Write-Host "first diff at line $($i + 1):"
        Write-Host "  ir2: $($a[$i])"
        Write-Host "  ir3: $($b[$i])"
        break
    }
}
exit 1
