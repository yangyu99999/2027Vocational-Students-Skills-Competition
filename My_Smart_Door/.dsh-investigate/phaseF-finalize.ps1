$ErrorActionPreference = 'Stop'
$Plague = 'E:\Deepseek Harness\Plague'
$HomeE  = Join-Path $Plague 'dsh-home'
$Caches = Join-Path $Plague 'caches'
$Log    = Join-Path $Plague 'logs\phaseF.log'
$C      = 'C:\Users\YangYu\.dsh'
$utf8   = New-Object System.Text.UTF8Encoding($false)
New-Item -ItemType Directory -Force -Path (Split-Path $Log) | Out-Null
function Log([string]$m) { $s = '[{0}] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $m; $s | Tee-Object -FilePath $Log -Append }
function Fail([string]$m) { Log ('FAILED: ' + $m); throw $m }
Log '=== Phase F start ==='

# --- safety: nothing may be running from the old C: home ---
$users = @()
foreach ($p in @(Get-Process -ErrorAction SilentlyContinue)) {
    try { if ($p.Path -and $p.Path.StartsWith($C, [StringComparison]::OrdinalIgnoreCase)) { $users += ($p.ProcessName + '#' + $p.Id) } } catch { }
}
if ($users.Count -gt 0) { Fail ('processes still use C:\.dsh: ' + ($users -join ',')) }
Log 'ok: no process uses C:\.dsh'

# --- 1. fix pnpm metadata paths inside migrated node_modules ---
$mods = Join-Path $HomeE 'profiles\desktop\node_modules\.modules.yaml'
if (Test-Path $mods) {
    $txt = [IO.File]::ReadAllText($mods)
    $c2 = $txt.Replace('C:\\Users\\YangYu\\AppData\\Local\\pnpm\\store', 'E:\\Deepseek Harness\\Plague\\caches\\pnpm-store')
    $c2 = $c2.Replace('C:\\Users\\YangYu\\.dsh', 'E:\\Deepseek Harness\\Plague\\dsh-home')
    if ($c2 -ne $txt) { [IO.File]::WriteAllText($mods, $c2, $utf8); Log 'fixed pnpm .modules.yaml paths' }
    else { Log '.modules.yaml already clean' }
}

# --- 2. scan top-level profile config files for old C: paths ---
$desk = Join-Path $HomeE 'profiles\desktop'
foreach ($name in @('package.json', 'pnpm-lock.yaml', 'pnpm-workspace.yaml', 'cordis.yml', 'cordis.patch.yml')) {
    $f = Join-Path $desk $name
    if (Test-Path $f) {
        $t = [IO.File]::ReadAllText($f)
        $t2 = $t.Replace('C:\Users\YangYu\.dsh', 'E:\Deepseek Harness\Plague\dsh-home')
        $t2 = $t2.Replace('C:/Users/YangYu/.dsh', 'E:/Deepseek Harness/Plague/dsh-home')
        if ($t2 -ne $t) { [IO.File]::WriteAllText($f, $t2, $utf8); Log ('fixed old paths in ' + $name) }
        else { Log ('clean ' + $name) }
    }
}

# --- 3. preserve the two small app-state files that were newer on C: ---
$bk = Join-Path $Plague 'backup\stale-state'
New-Item -ItemType Directory -Force -Path $bk | Out-Null
foreach ($rel in @('.dshw-turn.json', 'remote-web-ui-devices.json')) {
    $s = Join-Path $C $rel
    if (Test-Path $s) { Copy-Item $s (Join-Path $bk (Split-Path $rel -Leaf)) -Force; Log ('backed up stale ' + $rel) }
}

# --- 4. replace stale C:\.dsh with a junction to E: ---
$before = (Get-ChildItem $C -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum)
Log ("stale C home: {0} files {1:N1} MB" -f $before.Count, ($before.Sum / 1MB))
$empty = Join-Path $env:TEMP ('dsh-empty-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $empty | Out-Null
robocopy $empty $C /MIR /R:1 /W:1 /NP /NFL /NDL /NJH /NJS | Out-Null
Log ('purge robocopy rc=' + $LASTEXITCODE)
Remove-Item $empty -Force -Recurse -ErrorAction SilentlyContinue
if (Test-Path $C) { Remove-Item -LiteralPath $C -Force -Recurse -ErrorAction SilentlyContinue }
if (Test-Path $C) { cmd /c ('rmdir /s /q "' + $C + '"') | Out-Null }
if (Test-Path $C) { Fail 'C:\.dsh could not be removed completely' }
New-Item -ItemType Junction -Path $C -Target $HomeE | Out-Null
$it = Get-Item $C -Force
Log ('junction created: ' + $it.LinkType + ' -> ' + ($it.Target -join ','))
$cnt = @(Get-ChildItem $C -Force -ErrorAction SilentlyContinue).Count
Log ('items visible through junction: ' + $cnt)
if ($cnt -lt 10) { Fail 'junction does not resolve correctly' }

# --- 5. pet helper (only if not already linked and not locked) ---
$petSrc = Join-Path $env:APPDATA 'dsh-pet-electron-helper'
$petDst = Join-Path $Caches 'pet-helper'
if ((Test-Path $petSrc) -and -not (Get-Item $petSrc -Force).LinkType) {
    try {
        robocopy $petSrc $petDst /E /MOVE /XJ /R:1 /W:1 /NP /NFL /NDL /NJH /NJS | Out-Null
        Log ('pet-helper move rc=' + $LASTEXITCODE)
        if (Test-Path $petSrc) { cmd /c ('rmdir /s /q "' + $petSrc + '"') | Out-Null }
        if (-not (Test-Path $petSrc)) {
            New-Item -ItemType Junction -Path $petSrc -Target $petDst | Out-Null
            Log 'pet-helper -> junction created'
        } else { Log 'pet-helper still locked, left for finish script' }
    } catch { Log ('pet-helper skipped: ' + $_.Exception.Message) }
} else { Log 'pet-helper already linked or absent' }

# --- 6. refresh README status ---
$readme = @'
DSH 与开发缓存迁移目录  E:\Deepseek Harness\Plague
=====================================================
生成时间: 2026-10-27   状态: 已完成 ~99%（只剩最后 88MB，见下方第 2 节）

一、目录说明
  dsh-home\                 DSH 用户数据（原 C:\Users\<你>\.dsh）：插件、会话、配置、凭据、日志。
                            由用户环境变量 DSH_HOME 指向这里；C:\Users\<你>\.dsh 现在是一个
                            指向本目录的链接(Junction)，两种写法都会落到 E 盘。
  caches\npm\               npm 缓存                 (npm_config_cache)
  caches\pnpm-store\        pnpm 内容寻址仓库 (v11)   (npm_config_store_dir)
  caches\pnpm-cache\        pnpm 元数据缓存           (npm_config_cache_dir)
  caches\pnpm-state\        pnpm 状态                 (npm_config_state_dir)
  caches\npm-global\        全局 npm 包               (npm_config_prefix)，原 %APPDATA%\npm 已是链接
  caches\electron\          Electron 下载缓存         （原路径已是链接）
  caches\electron-updater\  DSH 更新包下载缓存        （原路径已是链接）
  caches\playwright*\       Playwright 浏览器缓存     （原路径已是链接）
  caches\electron-userdata\ Electron 应用数据 @deepseek-ai（待最后一步搬移）
  caches\pet-helper\        桌面宠物辅助程序           （原路径已是链接）
  caches\pip\               Python pip 缓存
  temp\                     预留（未启用，系统 TEMP 仍在 C 盘且体积很小）
  logs\                     迁移与诊断日志（phaseA~F）
  scripts\                  迁移脚本 / 回滚脚本 / archive\ 归档
  backup\                   环境变量、npmrc、PATH 的迁移前备份

二、最后一步（只差 88MB，需要完全退出 DSH 一次）
  1) 托盘图标右键 -> 退出（确保任务管理器里没有 DeepSeek Harness.exe）
  2) 双击桌面「完成DSH迁移到E盘」，脚本会：
       把 %APPDATA%\@deepseek-ai（Electron 窗口设置等 85MB）搬到 E:\...\caches\electron-userdata
       并在原位置建立链接；随后自动重启 DSH。
     （C 盘 .dsh 已完成搬迁与链接，脚本会自动跳过。）

三、回滚方法
  1) 运行 scripts\revert-env.ps1 删除本次设置的用户环境变量（原 PATH、原 .npmrc 在 backup\）。
  2) 删除各 Junction（只删链接、别删目标），再把 E 盘数据拷回 C 盘。

四、有意保留在 C 盘的内容
  - 系统 TEMP/TMP（Windows 与沙箱运行需要，体积小、可随时清）。
  - 与本代理无关的其它软件数据。
  - 原有的 CODEX_CACHE_DIR=E:\chatGPT\cache 本来就在 E 盘，未改动。
'@
[IO.File]::WriteAllText((Join-Path $Plague 'README.txt'), $readme, $utf8)
Log 'README refreshed'

Log '=== Phase F done ==='
