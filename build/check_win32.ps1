Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")] public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr hWnd, int nIndex);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    public const int GWL_STYLE = -16;
    public const int WS_OVERLAPPEDWINDOW = 0x00CF0000;
    public const int WS_THICKFRAME = 0x00040000;
    public const int WS_MAXIMIZEBOX = 0x00010000;
    public const int SW_MAXIMIZE = 3;
}
"@

$exe = "C:\Users\FoadS\Downloads\20261101\c99gui\wt-vector\build\tmp\s_win32.exe"
$proc = Start-Process $exe -PassThru
Start-Sleep -Seconds 2

$hwnd = [Win32]::FindWindow("C99VectorSuite", "Pure C99 Vector GUI Framework")
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host "FAIL: Window not found"
    $proc.Kill()
    exit 1
}

$style = [Win32]::GetWindowLong($hwnd, [Win32]::GWL_STYLE)
$hasThickFrame = ($style -band [Win32]::WS_THICKFRAME) -ne 0
$hasMaximizeBox = ($style -band [Win32]::WS_MAXIMIZEBOX) -ne 0
$hasOverlappedWindow = ($style -band [Win32]::WS_OVERLAPPEDWINDOW) -eq [Win32]::WS_OVERLAPPEDWINDOW

Write-Host "Window Style: 0x$([Convert]::ToString($style, 16))"
Write-Host "WS_THICKFRAME (0x40000): $hasThickFrame"
Write-Host "WS_MAXIMIZEBOX (0x10000): $hasMaximizeBox"
Write-Host "WS_OVERLAPPEDWINDOW: $hasOverlappedWindow"

$rectBefore = New-Object Win32+RECT
[Win32]::GetClientRect($hwnd, [ref]$rectBefore)
Write-Host "Client Rect Before Maximize: $($rectBefore.Right - $rectBefore.Left) x $($rectBefore.Bottom - $rectBefore.Top)"

[Win32]::ShowWindow($hwnd, [Win32]::SW_MAXIMIZE)
Start-Sleep -Seconds 1

$rectAfter = New-Object Win32+RECT
[Win32]::GetClientRect($hwnd, [ref]$rectAfter)
Write-Host "Client Rect After Maximize: $($rectAfter.Right - $rectAfter.Left) x $($rectAfter.Bottom - $rectAfter.Top)"

$proc.Kill()

if ($hasThickFrame -and $hasMaximizeBox -and $hasOverlappedWindow) {
    Write-Host "PASS: Window has correct styles"
    exit 0
} else {
    Write-Host "FAIL: Window missing required styles"
    exit 1
}