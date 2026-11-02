# Build matrix for c99gui: every available compiler on this machine,
# building all four programs, with warnings-as-errors, then run smoke tests.
param([string]$Only = '')
$ErrorActionPreference = 'Continue'
$Root = Split-Path -Parent $PSScriptRoot
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
    $o = & $GitBash -c "chmod +x tests/smoke.sh tests/tui_smoke.sh; sh tests/tui_smoke.sh '$p' 2>&1"
    $tuiOk = ($LASTEXITCODE -eq 0)
    
    $o = & $GitBash -c "sh tests/smoke.sh '$p-pixel.exe' 2>&1"
    $pixelOk = ($LASTEXITCODE -eq 0)
    
    $o = & $GitBash -c "sh tests/smoke.sh '$p-vector.exe' 2>&1"
    $vectorOk = ($LASTEXITCODE -eq 0)
    
    $o = & $GitBash -c "sh tests/smoke.sh '$p-suite.exe' 2>&1"
    $suiteOk = ($LASTEXITCODE -eq 0)
    
    return @{ Ok = ($tuiOk -and $pixelOk -and $vectorOk -and $suiteOk); Tail = "all tests passed" }
}

function Test-Config($name) { return ($Only -eq '' -or $name -like "*$Only*") }

# ---- MSVC (every installed Visual Studio, x64, default and headless) ----
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $installs = & $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    foreach ($vs in $installs) {
        $vsName = (Split-Path $vs -Parent | Split-Path -Leaf) + '-' + (Split-Path $vs -Leaf)
        $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
        foreach ($mode in 'default', 'headless') {
            $name = "msvc-$vsName-$mode"
            if (-not (Test-Config $name)) { continue }
            $dir = Join-Path $Out $name; New-Item -ItemType Directory -Force $dir | Out-Null
            $def = if ($mode -eq 'headless') { '/DC99GUI_HEADLESS' } else { '' }
            
            $cmd = "`"$vcvars`" amd64 >nul && " +
                   "cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 $def /Fo`"$dir\\`" /Fe`"$dir\c99gui-tui.exe`" `"$Root\src\tui\c99gui.c`" && " +
                   "cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 $def /Fo`"$dir\\`" /Fe`"$dir\c99gui-pixel.exe`" `"$Root\src\pixel\gui.c`" && " +
                   "cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 $def /Fo`"$dir\\`" /Fe`"$dir\c99gui-vector.exe`" `"$Root\src\vector\gui.c`" && " +
                   "cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 $def /Fo`"$dir\\`" /Fe`"$dir\c99gui-suite.exe`" `"$Root\src\suite\gui.c`""
            
            $log = cmd /c $cmd 2>&1
            if ($LASTEXITCODE -ne 0) { 
                Add-Result $name 'FAIL' '-' ($log | Select-Object -Last 3 | Out-String).Trim()
                continue 
            }
            
            $t = Invoke-SmokeNative "$dir\c99gui"
            Add-Result $name 'ok' $(if ($t.Ok) { 'pass' } else { 'FAIL' }) $t.Tail
        }
    }
}

# ---- MSYS2 toolchains (UCRT64 GCC) ------------------------------------------
foreach ($env_ in 'ucrt64') {
    $bin = "C:\msys64\$env_\bin"
    $cc = 'gcc'
    $exeCc = Join-Path $bin "$cc.exe"
    if (-not (Test-Path $exeCc)) { continue }
    foreach ($mode in 'default', 'headless') {
        $name = "msys2-$env_-$cc-$mode"
        if (-not (Test-Config $name)) { continue }
        $dir = Join-Path $Out $name; New-Item -ItemType Directory -Force $dir | Out-Null
        $def = if ($mode -eq 'headless') { '-DC99GUI_HEADLESS' } else { '' }
        $saved = $env:PATH; $env:PATH = "$bin;$env:PATH"
        
        # TUI
        $ccArgs = @($GccWarn -split ' '); if ($def) { $ccArgs += $def }
        $ccArgs += @("$Root\src\tui\c99gui.c", '-o', "$dir\c99gui-tui.exe")
        $log = & $exeCc @ccArgs 2>&1
        $rc = $LASTEXITCODE
        if ($rc -ne 0) { $env:PATH = $saved; Add-Result $name 'FAIL' '-' ($log | Select-Object -Last 3 | Out-String).Trim(); continue }
        
        # GUIs with GDI32
        $ccArgs = @($GccWarn -split ' '); if ($def) { $ccArgs += $def }
        $ccArgs += @("$Root\src\pixel\gui.c", '-lgdi32', '-luser32', '-o', "$dir\c99gui-pixel.exe")
        $log = & $exeCc @ccArgs 2>&1
        if ($LASTEXITCODE -ne 0) { $env:PATH = $saved; Add-Result $name 'FAIL' '-' ($log | Select-Object -Last 3 | Out-String).Trim(); continue }
        
        $ccArgs = @($GccWarn -split ' '); if ($def) { $ccArgs += $def }
        $ccArgs += @("$Root\src\vector\gui.c", '-lgdi32', '-luser32', '-o', "$dir\c99gui-vector.exe")
        $log = & $exeCc @ccArgs 2>&1
        if ($LASTEXITCODE -ne 0) { $env:PATH = $saved; Add-Result $name 'FAIL' '-' ($log | Select-Object -Last 3 | Out-String).Trim(); continue }
        
        $ccArgs = @($GccWarn -split ' '); if ($def) { $ccArgs += $def }
        $ccArgs += @("$Root\src\suite\gui.c", '-lgdi32', '-luser32', '-o', "$dir\c99gui-suite.exe")
        $log = & $exeCc @ccArgs 2>&1
        if ($LASTEXITCODE -ne 0) { $env:PATH = $saved; Add-Result $name 'FAIL' '-' ($log | Select-Object -Last 3 | Out-String).Trim(); continue }
        
        $env:PATH = "$bin;$env:PATH"
        $t = Invoke-SmokeNative "$dir\c99gui"
        $env:PATH = $saved
        Add-Result $name 'ok' $(if ($t.Ok) { 'pass' } else { 'FAIL' }) $t.Tail
    }
}

$Results | Format-Table -AutoSize -Wrap | Out-String -Width 200 | Write-Host
$bad = @($Results | Where-Object { $_.Built -ne 'skip' -and ($_.Built -ne 'ok' -or $_.Tests -ne 'pass') })
if ($bad.Count -gt 0) { Write-Host "$($bad.Count) configuration(s) failed" -ForegroundColor Red; exit 1 }
Write-Host "all $($Results.Count) configurations built and passed" -ForegroundColor Green
