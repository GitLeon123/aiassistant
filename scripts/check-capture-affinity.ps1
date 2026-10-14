# Read-only diagnostic. A successful affinity request is not a capture-tool test.
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class CaptureAffinityCheck {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr data);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder name, int size);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool GetWindowDisplayAffinity(IntPtr hwnd, out uint affinity);
    public static string[] Run() {
        var lines = new List<string>();
        EnumWindows((hwnd, data) => {
            var name = new StringBuilder(256);
            GetClassName(hwnd, name, name.Capacity);
            if (name.ToString() == "AIAssistant.Overlay") {
                uint affinity;
                bool ok = GetWindowDisplayAffinity(hwnd, out affinity);
                lines.Add(ok ? "Overlay affinity: 0x" + affinity.ToString("X") + (affinity == 0x11 ? " (exclusion requested)" : " (exclusion not requested)")
                             : "GetWindowDisplayAffinity failed: " + Marshal.GetLastWin32Error());
            }
            return true;
        }, IntPtr.Zero);
        if (lines.Count == 0) lines.Add("No AIAssistant overlay found. Start the app first.");
        return lines.ToArray();
    }
}
'@
[CaptureAffinityCheck]::Run()
Write-Output 'Display affinity is best-effort. Verify actual captures separately; it is not a security guarantee.'
