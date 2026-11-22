# Reads guest critical sections out of a running (frozen) CrimsonSkies.exe without attaching a debugger.
# Usage: powershell -File tools\guest_locks.ps1 [-Base 0x3133F8] [-Count 12] [-Stride 0x1C]
# Guest memory is mapped at 0x100000000 (the log's "memory: guest space at" line). An Xbox RTL_CRITICAL_SECTION is
# 0x1C bytes: event header 0x10, LockCount +0x10, RecursionCount +0x14, OwningThread +0x18 (an ETHREAD; its
# UniqueThread at +0x12C is the id the log's "thread N starts" lines use).
param([long]$Base = 0x3133F8, [int]$Count = 12, [int]$Stride = 0x1C, [long]$GuestBase = 0x100000000)
Add-Type @'
using System; using System.Runtime.InteropServices;
public class GuestMem {
    [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(int access, bool inherit, int pid);
    [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr h, long addr, byte[] buf, int n, out int read);
}
'@
$p = Get-Process CrimsonSkies -ErrorAction Stop | Select-Object -First 1
$h = [GuestMem]::OpenProcess(0x0410, $false, $p.Id)  # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
function Read-Guest32([long]$va) {
    $b = New-Object byte[] 4; $n = 0
    [void][GuestMem]::ReadProcessMemory($h, $GuestBase + $va, $b, 4, [ref]$n)
    [BitConverter]::ToUInt32($b, 0)
}
"pid $($p.Id)"
for ($i = 0; $i -lt $Count; $i++) {
    $cs = $Base + $i * $Stride
    $owner = Read-Guest32 ($cs + 0x18)
    $tid = if ($owner) { '{0:X}' -f (Read-Guest32 ($owner + 0x12C)) } else { '-' }
    $lock = [BitConverter]::ToInt32([BitConverter]::GetBytes([uint32](Read-Guest32 ($cs + 0x10))), 0)  # -1 = free
    "cs[{0}] @{1:X}: lock {2} recursion {3} owner {4:X8} (thread {5})" -f $i, $cs, $lock,
        (Read-Guest32 ($cs + 0x14)), $owner, $tid
}
