# Pre-seed E:\chatGPT with copies of the Codex data (Codex app keeps running during this phase).
# The final switch (move + junction) is done later by codex-switch.ps1 while Codex is closed.
$ErrorActionPreference = 'Continue'
$Log = 'E:\chatGPT\migration\preseed.log'
New-Item -ItemType Directory -Force -Path 'E:\chatGPT\migration' | Out-Null
function L([string]$m) { ('[{0}] {1}' -f (Get-Date -Format 'HH:mm:ss'), $m) | Tee-Object -FilePath $Log -Append | Out-Null }
L ("preseed start; C free={0:N1}GB E free={1:N1}GB" -f ((Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'").FreeSpace/1GB), ((Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='E:'").FreeSpace/1GB))

# 1) instant same-volume renames of the copies Codex made earlier into the final target layout
$renames = @(
    @{ S = 'E:\chatGPT\codex-data\codex-primary-runtime'; D = 'E:\chatGPT\cache\codex-runtimes\codex-primary-runtime' },
    @{ S = 'E:\chatGPT\codex-data\codex-runtimes';        D = 'E:\chatGPT\codex-app\Codex\runtimes' }
)
foreach ($r in $renames) {
    if (-not (Test-Path -LiteralPath $r.S)) { L ('rename source absent: ' + $r.S); continue }
    if (Test-Path -LiteralPath $r.D) { L ('rename target exists, skip: ' + $r.D); continue }
    $parent = Split-Path -Parent $r.D
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    try { Move-Item -LiteralPath $r.S -Destination $r.D -ErrorAction Stop; L ('renamed ' + $r.S + ' -> ' + $r.D) }
    catch { L ('rename failed: ' + $_.Exception.Message) }
}

# 2) robocopy pre-seed (copy only; source untouched)
$jobs = @(
    @{ S = 'C:\Users\YangYu\.codex';                                                        D = 'E:\chatGPT\codex-home';                       N = 'codex-home' },
    @{ S = 'C:\Users\YangYu\.cache\codex-runtimes';                                         D = 'E:\chatGPT\cache\codex-runtimes';             N = 'codex-runtimes' },
    @{ S = 'C:\Users\YangYu\AppData\Local\OpenAI\Codex';                                    D = 'E:\chatGPT\codex-app\Codex';                  N = 'local-openai-codex' },
    @{ S = 'C:\Users\YangYu\.codex-session-delete';                                         D = 'E:\chatGPT\codex-session-delete';             N = 'codex-session-delete' },
    @{ S = 'C:\Users\YangYu\AppData\Local\CodexDreamSkin';                                  D = 'E:\chatGPT\codex-plugins-data\CodexDreamSkin'; N = 'CodexDreamSkin' },
    @{ S = 'C:\Users\YangYu\AppData\Local\com.bigpizzav3.codexplusplus.manager';            D = 'E:\chatGPT\codex-plugins-data\com.bigpizzav3.codexplusplus.manager'; N = 'codexplusplus' },
    @{ S = 'C:\Users\YangYu\AppData\Roaming\Codex++';                                       D = 'E:\chatGPT\codex-plugins-data\Codex++';       N = 'Codex++' }
)
foreach ($j in $jobs) {
    if (-not (Test-Path -LiteralPath $j.S)) { L ('absent: ' + $j.S); continue }
    New-Item -ItemType Directory -Force -Path $j.D | Out-Null
    $t0 = Get-Date
    robocopy $j.S $j.D /E /XJ /R:1 /W:1 /MT:16 /NP /NFL /NDL /NJH /NJS | Out-Null
    $rc = $LASTEXITCODE
    L ('{0}: robocopy rc={1} in {2:N0}s  {3} -> {4}' -f $j.N, $rc, ((Get-Date) - $t0).TotalSeconds, $j.S, $j.D)
}
L 'preseed done'
L ("C free={0:N1}GB E free={1:N1}GB" -f ((Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'").FreeSpace/1GB), ((Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='E:'").FreeSpace/1GB))
