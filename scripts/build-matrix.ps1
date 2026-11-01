# Build matrix for c99gui Desktop Pixel GUI: every available compiler on this machine,
# with warnings-as-errors, then run tests/smoke.sh against each binary.
param([string]$Only = '')
$ErrorActionPreference = 'Continue'
$Root = Split-Path -Parent $PSScriptRoot
$Src = Join-Path $Root 'src\gui.c'
$Out = Join-Path $Root 'build'
$GitBash = 'C:\Program Files\Git\bin\bash.exe'
$Results = New-Object System.Collections.Generic.List[object]
$GccWarn = '-std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror'

function Add-Result($name, $built, $tested, $note) {
    $Results.Add([pscustomobject]@{ Config = $name; Built = $built; Tests = $tested; Note = $note })
}

function Invoke-SmokeNative($exe) {
    $env:PATH = 'C:\Program Files\Git\usr\bin;' + $env:PATH
    $p = $exe -replace '\\', '/'
    $o = & $GitBash 'tests/smoke.sh' $p 2>&1
    return @{ Ok = ($LASTEXITCODE -eq 0); Tail = ($o | Select-Object -Last 1) }
}

function Test-Config($name) { return ($Only -eq '' -or $name -like "*$Only*") }

# ---- MSVC (every installed Visual Studio, x64 + x86, default and headless) ----
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $installs = & $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    foreach ($vs in $installs) {
        $vsName = (Split-Path $vs -Parent | Split-Path -Leaf) + '-' + (Split-Path $vs -Leaf)
        $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
        foreach ($arch in 'x64', 'x86') {
            foreach ($mode in 'default', 'headless') {
                $name = "msvc-$vsName-$arch-$mode"
                if (-not (Test-Config $name)) { continue }
                $dir = Join-Path $Out $name; New-Item -ItemType Directory -Force $dir | Out-Null
                $def = if ($mode -eq 'headless') { '/DC99GUI_HEADLESS' } else { '' }
                $exe = Join-Path $dir 'gui.exe'
                $cmd = "`"$vcvars`" $arch >nul && cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 $def /Fo`"$dir\\`" /Fe`"$exe`" `"$Src`""
                $log = cmd /c $cmd 2>&1
                if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { Add-Result $name 'FAIL' '-' ($log | Select-Object -Last 3 | Out-String).Trim(); continue }
                $t = Invoke-SmokeNative $exe
                Add-Result $name 'ok' $(if ($t.Ok) { 'pass' } else { 'FAIL' }) $t.Tail
            }
        }
    }
}

# ---- MSYS2 toolchains (UCRT64 GCC) ------------------------------------------
foreach ($env_ in 'ucrt64', 'mingw64', 'clang64') {
    $bin = "C:\msys64\$env_\bin"
    foreach ($cc in 'gcc', 'clang') {
        $exeCc = Join-Path $bin "$cc.exe"
        if (-not (Test-Path $exeCc)) { continue }
        foreach ($mode in 'default', 'headless') {
            $name = "msys2-$env_-$cc-$mode"
            if (-not (Test-Config $name)) { continue }
            $dir = Join-Path $Out $name; New-Item -ItemType Directory -Force $dir | Out-Null
            $exe = Join-Path $dir 'gui.exe'
            $def = if ($mode -eq 'headless') { '-DC99GUI_HEADLESS' } else { '' }
            $saved = $env:PATH; $env:PATH = "$bin;$env:PATH"
            $ccArgs = @($GccWarn -split ' '); if ($def) { $ccArgs += $def }
            $ccArgs += @($Src, '-lgdi32', '-luser32', '-o', $exe)
            $log = & $exeCc @ccArgs 2>&1
            $rc = $LASTEXITCODE; $env:PATH = $saved
            if ($rc -ne 0) { Add-Result $name 'FAIL' '-' ($log | Select-Object -Last 3 | Out-String).Trim(); continue }
            $env:PATH = "$bin;$env:PATH"; $t = Invoke-SmokeNative $exe; $env:PATH = $saved
            Add-Result $name 'ok' $(if ($t.Ok) { 'pass' } else { 'FAIL' }) $t.Tail
        }
    }
}

# ---- Cygwin (GCC and Clang with GDI32) ---------------------------------------
$cyg = 'C:\cygwin64\bin\bash.exe'
if (Test-Path $cyg) {
    $cygRoot = (& 'C:\cygwin64\bin\cygpath.exe' -u $Root)
    foreach ($cc in 'gcc', 'clang') {
        & $cyg -lc "command -v $cc >/dev/null" *> $null
        if ($LASTEXITCODE -ne 0) { Add-Result "cygwin-$cc" 'skip' '-' 'compiler not installed'; continue }
        foreach ($mode in 'default', 'headless') {
            $cfg = "cygwin-$cc-$mode"
            if (-not (Test-Config $cfg)) { continue }
            $def = if ($mode -eq 'headless') { '-DC99GUI_HEADLESS' } else { '' }
            $script = "set -e; cd `"$cygRoot`"; mkdir -p build/$cfg; " +
                      "$cc $GccWarn $def src/gui.c -lgdi32 -luser32 -o build/$cfg/gui.exe; sh tests/smoke.sh build/$cfg/gui.exe | tail -1"
            $o = & $cyg -lc $script 2>&1
            $ok = ($LASTEXITCODE -eq 0)
            if ($ok) { Add-Result $cfg 'ok' 'pass' ($o | Select-Object -Last 1) }
            else { Add-Result $cfg 'FAIL' '-' (($o | Select-Object -Last 3) -join ' ') }
        }
    }
}

# ---- WSL Ubuntu (GCC) -------------------------------------------------------
$env:WSL_UTF8 = '1'
if (Get-Command wsl -ErrorAction SilentlyContinue) {
    $wslRoot = ($Root -replace '^([A-Za-z]):', { '/mnt/' + $_.Groups[1].Value.ToLower() }) -replace '\\', '/'
    foreach ($cc in 'gcc', 'clang') {
        wsl -d Ubuntu -- bash -lc "command -v $cc >/dev/null" *> $null
        if ($LASTEXITCODE -ne 0) { Add-Result "wsl-ubuntu-$cc" 'skip' '-' 'compiler not installed'; continue }
        foreach ($mode in 'default', 'headless') {
            $cfg = "wsl-ubuntu-$cc-$mode"
            if (-not (Test-Config $cfg)) { continue }
            $def = if ($mode -eq 'headless') { '-DC99GUI_HEADLESS' } else { '' }
            $script = "set -e; cd `"$wslRoot`"; mkdir -p build/$cfg; " +
                      "$cc $GccWarn $def src/gui.c -o build/$cfg/gui; sh tests/smoke.sh build/$cfg/gui | tail -1"
            $o = wsl -d Ubuntu -- bash -lc $script 2>&1
            $ok = ($LASTEXITCODE -eq 0)
            if ($ok) { Add-Result $cfg 'ok' 'pass' ($o | Select-Object -Last 1) }
            else { Add-Result $cfg 'FAIL' '-' (($o | Select-Object -Last 3) -join ' ') }
        }
    }
}

$Results | Format-Table -AutoSize -Wrap | Out-String -Width 200 | Write-Host
$bad = @($Results | Where-Object { $_.Built -ne 'skip' -and ($_.Built -ne 'ok' -or $_.Tests -ne 'pass') })
if ($bad.Count -gt 0) { Write-Host "$($bad.Count) configuration(s) failed" -ForegroundColor Red; exit 1 }
Write-Host "all $($Results.Count) configurations built and passed" -ForegroundColor Green
