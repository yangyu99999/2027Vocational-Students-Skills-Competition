# Final switch: close Codex, move C: data to E:\chatGPT, leave junctions, set CODEX_HOME, restart Codex.
$ErrorActionPreference = 'Continue'
$Log = 'E:\chatGPT\migration\switch.log'
$C = 'C:\Users\YangYu'
function L([string]$m) { ('[{0}] {1}' -f (Get-Date -Format 'HH:mm:ss'), $m) | Tee-Object -FilePath $Log -Append | Out-Null }
L '================ switch start ================'

$svc = 'CodexSandboxService.OpenAI.Codex'
$svcState = (Get-Service -Name $svc -ErrorAction SilentlyContinue).Status
L ("service initial state: " + $svcState)

# ---------- 1. gracefully close Codex ----------
$main = @(Get-Process -Name 'ChatGPT' -ErrorAction SilentlyContinue)
foreach ($p in $main) { try { if ($p.MainWindowHandle -ne 0) { [void]$p.CloseMainWindow() } } catch { } }
L ('CloseMainWindow sent to ' + $main.Count + ' ChatGPT process(es); waiting up to 25s')
$deadline = (Get-Date).AddSeconds(25)
while ((Get-Date) -lt $deadline) {
    if (-not (Get-Process -Name 'ChatGPT' -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Milliseconds 800
}
# stop the sandbox service before force-killing leftovers
try { Stop-Service -Name $svc -Force -ErrorAction Stop; L 'sandbox service stopped' } catch { L ('stop service failed: ' + $_.Exception.Message) }

$killNames = @('ChatGPT', 'codex', 'codex-code-mode-host', 'codex-computer-use-swift', 'node_repl')
$deadline = (Get-Date).AddSeconds(25)
while ($true) {
    $left = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
            $killNames -contains $_.ProcessName -or
            ($_.Path -and ($_.Path -match '\\OpenAI\\Codex\\' -or $_.Path -match '\\codex-runtimes\\'))
        })
    if ($left.Count -eq 0) { break }
    if ((Get-Date) -ge $deadline) {
        L ('force killing: ' + (($left | ForEach-Object { $_.ProcessName + '#' + $_.Id }) -join ','))
        foreach ($p in $left) { try { Stop-Process -Id $p.Id -Force -ErrorAction Stop } catch { } }
        Start-Sleep -Seconds 4
    } else { Start-Sleep -Milliseconds 700 }
}
$still = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $killNames -contains $_.ProcessName -or ($_.Path -and ($_.Path -match '\\OpenAI\\Codex\\' -or $_.Path -match '\\codex-runtimes\\')) })
if ($still.Count -gt 0) {
    L ('ABORT: processes still alive: ' + (($still | ForEach-Object { $_.ProcessName + '#' + $_.Id }) -join ','))
    exit 1
}
L 'all Codex processes are gone'

# ---------- 2. move + junction each C: directory ----------
function Remove-LongPath([string]$p) {
    $empty = Join-Path $env:TEMP ('rmdir-empty-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $empty | Out-Null
    robocopy $empty $p /MIR /NFL /NDL /NJH /NJS /R:1 /W:1 | Out-Null
    Start-Sleep -Milliseconds 400
    cmd /c ('rmdir /s /q "' + $p + '"') | Out-Null
    Remove-Item -LiteralPath $empty -Recurse -Force -ErrorAction SilentlyContinue
    return (-not (Test-Path -LiteralPath $p))
}

# link-target is the directory that receives the junction; usually same as dest
$moves = @(
    @{ S = "$C\.codex";                                                    D = 'E:\chatGPT\codex-home';                    L = "$C\.codex";                                    N = 'codex-home' },
    @{ S = "$C\.cache\codex-runtimes";                                      D = 'E:\chatGPT\cache\codex-runtimes';          L = "$C\.cache\codex-runtimes";                      N = 'codex-runtimes' },
    @{ S = "$C\AppData\Local\OpenAI\Codex";                                 D = 'E:\chatGPT\codex-app\Codex';               L = "$C\AppData\Local\OpenAI";                       N = 'local-openai' },
    @{ S = "$C\.codex-session-delete";                                      D = 'E:\chatGPT\codex-session-delete';          L = "$C\.codex-session-delete";                      N = 'session-delete' },
    @{ S = "$C\AppData\Local\CodexDreamSkin";                               D = 'E:\chatGPT\codex-plugins-data\CodexDreamSkin'; L = "$C\AppData\Local\CodexDreamSkin";            N = 'dreamskin' },
    @{ S = "$C\AppData\Local\com.bigpizzav3.codexplusplus.manager";         D = 'E:\chatGPT\codex-plugins-data\com.bigpizzav3.codexplusplus.manager'; L = "$C\AppData\Local\com.bigpizzav3.codexplusplus.manager"; N = 'codexplusplus' },
    @{ S = "$C\AppData\Roaming\Codex++";                                    D = 'E:\chatGPT\codex-plugins-data\Codex++';     L = "$C\AppData\Roaming\Codex++";                   N = 'Codex++' }
)

$results = @()
foreach ($m in $moves) {
    if (-not (Test-Path -LiteralPath $m.S)) { L ('SKIP (absent): ' + $m.S); $results += ($m.N + ':absent'); continue }
    $linkItem = Get-Item -LiteralPath $m.L -Force
    if ($linkItem.LinkType) { L ('SKIP (already junction): ' + $m.L); $results += ($m.N + ':already-link'); continue }
    New-Item -ItemType Directory -Force -Path $m.D | Out-Null
    $t0 = Get-Date
    robocopy $m.S $m.D /MIR /XJ /R:2 /W:2 /MT:16 /NP /NFL /NDL /NJH /NJS | Out-Null
    $rc = $LASTEXITCODE
    L ('{0}: robocopy rc={1} in {2:N0}s' -f $m.N, $rc, ((Get-Date) - $t0).TotalSeconds)
    if ($rc -ge 8) { L ('FAIL copy, leaving C intact: ' + $m.S); $results += ($m.N + ':copy-fail'); continue }
    $srcFiles = @(Get-ChildItem -LiteralPath $m.S -Recurse -File -Force -ErrorAction SilentlyContinue)
    $dstFiles = @(Get-ChildItem -LiteralPath $m.D -Recurse -File -Force -ErrorAction SilentlyContinue)
    $srcSum = ($srcFiles | Measure-Object -Property Length -Sum).Sum
    $dstSum = ($dstFiles | Measure-Object -Property Length -Sum).Sum
    L ('{0}: src {1} files/{2:N1}MB  dst {3} files/{4:N1}MB' -f $m.N, $srcFiles.Count, ($srcSum/1MB), $dstFiles.Count, ($dstSum/1MB))
    if ($dstFiles.Count -lt $srcFiles.Count -or $dstSum -lt ($srcSum * 0.999)) { L ('FAIL verify counts, leaving C intact: ' + $m.S); $results += ($m.N + ':verify-fail'); continue }
    if (-not (Remove-LongPath $m.S)) { L ('FAIL could not remove source: ' + $m.S); $results += ($m.N + ':delete-fail'); continue }
    cmd /c ('mklink /J "' + $m.L + '" "' + $m.D + '"') | Out-Null
    if ($LASTEXITCODE -eq 0) { L ('OK junction: ' + $m.L + ' -> ' + $m.D); $results += ($m.N + ':ok') }
    else { L ('FAIL junction: ' + $m.L); $results += ($m.N + ':junction-fail') }
}

# ---------- 3. proactive junction for the app's Chromium user-data dir ----------
$addr = Join-Path $env:APPDATA 'Codex'
$addrTarget = 'E:\chatGPT\codex-appdata\Codex'
if (-not (Test-Path -LiteralPath $addr)) {
    New-Item -ItemType Directory -Force -Path $addrTarget | Out-Null
    cmd /c ('mklink /J "' + $addr + '" "' + $addrTarget + '"') | Out-Null
    L ('proactive junction for future app data: ' + $addr + ' (rc=' + $LASTEXITCODE + ')')
}

# ---------- 4. user env CODEX_HOME ----------
[Environment]::SetEnvironmentVariable('CODEX_HOME', 'E:\chatGPT\codex-home', 'User')
L ('CODEX_HOME(user) = ' + [Environment]::GetEnvironmentVariable('CODEX_HOME', 'User'))
try {
    Add-Type -MemberDefinition '[DllImport("user32.dll", SetLastError=true)] public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out UIntPtr lpdwResult);' -Name Env -Namespace Win32 -ErrorAction Stop
    $res = [UIntPtr]::Zero
    [void][Win32.Env]::SendMessageTimeout([IntPtr]0xffff, 0x001A, [UIntPtr]::Zero, 'Environment', 2, 5000, [ref]$res)
    L 'WM_SETTINGCHANGE broadcast'
} catch { L ('broadcast failed: ' + $_.Exception.Message) }

# ---------- 5. cleanup of the old duplicated copy ----------
if (Test-Path -LiteralPath 'E:\chatGPT\codex-data') {
    $left = @(Get-ChildItem 'E:\chatGPT\codex-data' -Force -ErrorAction SilentlyContinue)
    L ('codex-data leftover entries: ' + ($left.Name -join ','))
    if ((Test-Path -LiteralPath 'E:\chatGPT\codex-home') -and (Test-Path -LiteralPath 'E:\chatGPT\cache\codex-runtimes') -and (Test-Path -LiteralPath 'E:\chatGPT\codex-app\Codex\runtimes')) {
        if (Remove-LongPath 'E:\chatGPT\codex-data') { L 'removed duplicated E:\chatGPT\codex-data' } else { L 'could not remove codex-data' }
    }
}

# ---------- 6. restart service + Codex ----------
try { Start-Service -Name $svc -ErrorAction Stop; L ('service restarted: ' + (Get-Service -Name $svc).Status) } catch { L ('start service failed: ' + $_.Exception.Message) }

$pkg = Get-AppxPackage -Name 'OpenAI.Codex' -ErrorAction SilentlyContinue | Select-Object -First 1
$launched = $false
if ($pkg) {
    $aumid = $pkg.PackageFamilyName + '!App'
    L ('AUMID guess: ' + $aumid)
    try { Start-Process explorer.exe ('shell:AppsFolder\' + $aumid) -ErrorAction Stop; $launched = $true } catch { L ('launch via AUMID failed: ' + $_.Exception.Message) }
}
if (-not $launched) {
    $exe = 'C:\Program Files\WindowsApps\OpenAI.Codex_26.1023.6621.0_x64__2p2nqsd0c76g0\app\ChatGPT.exe'
    try { Start-Process $exe; $launched = $true } catch { L ('launch exe failed: ' + $_.Exception.Message) }
}
L 'waiting 30s for Codex to come up...'
Start-Sleep -Seconds 30
$procs = @(Get-Process -Name 'ChatGPT' -ErrorAction SilentlyContinue)
$win = @($procs | Where-Object { $_.MainWindowTitle } | ForEach-Object { $_.MainWindowTitle })
L ('ChatGPT processes after launch: ' + $procs.Count + '; window titles: ' + ($win -join ' | '))
L ('codex.exe present: ' + [bool](Get-Process -Name 'codex' -ErrorAction SilentlyContinue))
L ('check junctions:')
foreach ($m in $moves) {
    $it = Get-Item -LiteralPath $m.L -Force -ErrorAction SilentlyContinue
    L ('  {0}  LinkType={1}  Target={2}' -f $m.L, $it.LinkType, ($it.Target -join ','))
}
L '================ switch done ================'
L ('results: ' + ($results -join ' '))
