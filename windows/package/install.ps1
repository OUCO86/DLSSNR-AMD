# DLSSNR-AMD Windows installer (run by install.bat)
#
#   install.bat                      a window asks for the game's exe, then for the route
#   install.bat <game exe or its folder> [optiscaler|reshade|dx9|remove|logs]
#
# Every file put into the game folder is listed in dlssnr-amd-install.txt; existing files it would
# overwrite are first moved to dlssnr-amd-backup\, and uninstall deletes ours and puts them back.
param([string]$Target = '', [string]$Route = '', [switch]$Pause)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$ManifestName = 'dlssnr-amd-install.txt'
$BackupName = 'dlssnr-amd-backup'

function Say([string]$text) { Write-Host $text }
# Re-run as administrator it is a separate window: pause before it closes so the result can be read.
function Finish([int]$code) { if ($Pause) { Read-Host 'Press Enter to close' | Out-Null }; exit $code }
function Fail([string]$text) { Write-Host $text -ForegroundColor Red; Finish 1 }

# ---- game folder -----------------------------------------------------------------------------
if (-not $Target) {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = "Choose the game's exe (the one that actually runs, not the launcher)"
    $dialog.Filter = 'Game program (*.exe)|*.exe'
    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { Fail 'No game chosen.' }
    $Target = $dialog.FileName
}
$Target = $Target.Trim('"')
if (Test-Path -LiteralPath $Target -PathType Leaf) {
    $exe = (Resolve-Path -LiteralPath $Target).Path
    $game = Split-Path -Parent $exe
} elseif (Test-Path -LiteralPath $Target -PathType Container) {
    $game = (Resolve-Path -LiteralPath $Target).Path
    $exe = Get-ChildItem -LiteralPath $game -Filter *.exe | Select-Object -First 1 -ExpandProperty FullName
} else {
    Fail "Not found: $Target"
}
$manifest = Join-Path $game $ManifestName
$backup = Join-Path $game $BackupName

function Get-ExeBits([string]$path) {
    try {
        $b = [System.IO.File]::ReadAllBytes($path)
        $pe = [BitConverter]::ToInt32($b, 0x3C)
        switch ([BitConverter]::ToUInt16($b, $pe + 4)) { 0x8664 { return 64 } 0x14C { return 32 } }
    } catch { }
    return 0
}

# ---- choose the route ------------------------------------------------------------------------
if (-not $Route) {
    Say ''
    Say "Game folder: $game"
    Say 'Choose a route:'
    Say '  1) OptiScaler   the game has a DLSS, FSR or XeSS option (DX12 games only; for DX11 games choose 2)'
    Say '  2) ReShade      other DX10/11/12 or Vulkan games'
    Say '  3) ReShade      old DX9 games'
    Say '  4) Uninstall'
    Say '  5) Collect logs when something goes wrong; the zip goes to the desktop'
    $pick = Read-Host '1-5'
    switch ($pick) {
        '1' { $Route = 'optiscaler' } '2' { $Route = 'reshade' } '3' { $Route = 'dx9' }
        '4' { $Route = 'remove' } '5' { $Route = 'logs' }
        default { Fail 'Invalid choice.' }
    }
}
if ('optiscaler', 'reshade', 'dx9', 'remove', 'logs' -notcontains $Route) { Fail "Unknown route: $Route" }

# ---- games under Program Files need administrator rights -------------------------------------
if ($Route -ne 'logs') {
    $probe = Join-Path $game ('dlssnr-amd-write-test-' + [guid]::NewGuid().ToString('N'))
    try { [System.IO.File]::WriteAllText($probe, ''); Remove-Item -LiteralPath $probe -Force }
    catch {
        $me = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
        if ($me.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { Fail "Cannot write to $game" }
        Say 'This folder needs administrator rights, asking for them...'
        $elevated = "-NoProfile -ExecutionPolicy Bypass -STA -File `"$PSCommandPath`" -Target `"$game`" -Route $Route -Pause"
        $p = Start-Process powershell -Verb RunAs -ArgumentList $elevated -PassThru -Wait
        exit $p.ExitCode
    }
}

# ---- installation record ---------------------------------------------------------------------
function Record([string]$line) { Add-Content -LiteralPath $manifest -Value $line -Encoding UTF8 }

# A target that exists and was not installed by us: move it to the backup folder, put back on uninstall.
function Save-Existing([string]$rel) {
    $dst = Join-Path $game $rel
    if (-not (Test-Path -LiteralPath $dst)) { return }
    $keep = Join-Path $backup $rel
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $keep) | Out-Null
    Move-Item -LiteralPath $dst -Destination $keep -Force
    Record "B $rel"
}
function Put-File([string]$src, [string]$rel) {
    Save-Existing $rel
    Copy-Item -LiteralPath $src -Destination (Join-Path $game $rel) -Force
    Unblock-File -LiteralPath (Join-Path $game $rel) -ErrorAction SilentlyContinue
    Record "F $rel"
}
function Put-Tree([string]$src, [string]$rel) {
    Save-Existing $rel
    Copy-Item -LiteralPath $src -Destination (Join-Path $game $rel) -Recurse -Force
    Get-ChildItem -LiteralPath (Join-Path $game $rel) -Recurse -File | Unblock-File -ErrorAction SilentlyContinue
    Record "D $rel"
}

function Remove-Installed {
    if (-not (Test-Path -LiteralPath $manifest)) { Say 'No installation record found, nothing to uninstall.'; return }
    $lines = @(Get-Content -LiteralPath $manifest -Encoding UTF8)
    [array]::Reverse($lines)
    foreach ($line in $lines) {
        $line = $line.TrimStart([char]0xFEFF)
        if ($line.Length -lt 3) { continue }
        $kind = $line.Substring(0, 1); $rel = $line.Substring(2)
        if ($rel -match '(^|[\\/])\.\.([\\/]|$)' -or [System.IO.Path]::IsPathRooted($rel)) { Say "Skipping suspicious entry: $rel"; continue }
        $path = Join-Path $game $rel
        switch ($kind) {
            { 'F', 'D', 'L' -contains $_ } { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force } }
            'B' {
                $keep = Join-Path $backup $rel
                if (Test-Path -LiteralPath $keep) {
                    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
                    Move-Item -LiteralPath $keep -Destination $path -Force
                }
            }
        }
    }
    if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest -Force }
    if (Test-Path -LiteralPath $backup) {
        if (@(Get-ChildItem -LiteralPath $backup -Recurse -File).Count -eq 0) { Remove-Item -LiteralPath $backup -Recurse -Force }
        else { Say "Note: files in $backup were not put back; please check them by hand." }
    }
    Say "Uninstalled from $game."
}

# Files the game creates while running; removed on uninstall as well.
function Record-Logs([string[]]$names) { foreach ($n in $names) { Record "L $n" } }

# ---- collect logs ----------------------------------------------------------------------------
if ($Route -eq 'logs') {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $tmp = Join-Path $env:TEMP "dlssnr-amd-logs-$stamp"
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    $patterns = 'dlssnr-amd.log', 'dlssnr-amd.ini', $ManifestName, 'OptiScaler.log', 'OptiScaler.ini',
                'ReShade.log', 'ReShade.ini', 'ReShadePreset.ini', '*_dxgi.log', '*_d3d11.log', '*_d3d9.log',
                '*_d3d12.log', 'vkd3d-proton.log', 'dlssnr-amd-crash.dmp'
    foreach ($p in $patterns) {
        Get-ChildItem -LiteralPath $game -Filter $p -File -ErrorAction SilentlyContinue | Copy-Item -Destination $tmp
    }
    $info = @("game: $game", "exe: $exe", "exe bits: $(Get-ExeBits $exe)", "package: $here", '')
    $info += Get-CimInstance Win32_VideoController | ForEach-Object { "GPU: $($_.Name) driver $($_.DriverVersion) ($($_.DriverDate))" }
    $info += Get-CimInstance Win32_OperatingSystem | ForEach-Object { "OS: $($_.Caption) $($_.Version)" }
    $info += ''
    $info += Get-ChildItem -LiteralPath $game | ForEach-Object { '{0,12} {1}' -f $_.Length, $_.Name }
    $info | Set-Content -LiteralPath (Join-Path $tmp 'system.txt') -Encoding UTF8

    # The last two days of graphics driver events (driver stopped responding and recovered = 4101) and game crashes.
    $since = (Get-Date).AddDays(-2)
    $events = @()
    try {
        # Every error and warning in the System log (a driver reset is not always logged as 4101).
        $events += Get-WinEvent -FilterHashtable @{ LogName = 'System'; StartTime = $since; Level = 1, 2, 3 } -MaxEvents 300 -ErrorAction Stop
    } catch { }
    try {
        $events += Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $since; Id = 1000, 1001, 1002 } -ErrorAction Stop |
            Where-Object { $_.Message -match [regex]::Escape([System.IO.Path]::GetFileName($exe)) -or $_.Message -match 'LiveKernelEvent' }
    } catch { }
    $events | Sort-Object TimeCreated | ForEach-Object {
        "[$($_.TimeCreated.ToString('yyyy-MM-dd HH:mm:ss'))] $($_.LogName) $($_.ProviderName) id=$($_.Id)`r`n$($_.Message)`r`n"
    } | Set-Content -LiteralPath (Join-Path $tmp 'events.txt') -Encoding UTF8

    # File permissions: for finding out why ReShade says it cannot save its settings.
    $perm = @("user: $([Security.Principal.WindowsIdentity]::GetCurrent().Name)",
              "admin: $(([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))", '')
    foreach ($name in '.', 'ReShade.ini', 'ReShadePreset.ini', 'dlssnr-amd.ini') {
        $path = Join-Path $game $name
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $item = Get-Item -LiteralPath $path -Force
        $perm += "== $name  attributes: $($item.Attributes)"
        $perm += (& icacls.exe $path 2>&1 | Out-String)
    }
    $perm | Set-Content -LiteralPath (Join-Path $tmp 'permissions.txt') -Encoding UTF8
    $zip = Join-Path ([Environment]::GetFolderPath('Desktop')) "dlssnr-amd-logs-$stamp.zip"
    Compress-Archive -Path (Join-Path $tmp '*') -DestinationPath $zip -Force
    Remove-Item -LiteralPath $tmp -Recurse -Force
    Say "Logs packed: $zip"
    Finish 0
}

if ($Route -eq 'remove') { Remove-Installed; Finish 0 }

# ---- install ---------------------------------------------------------------------------------
$bits = Get-ExeBits $exe
if ($bits -eq 32) { Fail "This is a 32-bit game ($exe); this package supports 64-bit games only." }
if (-not (Test-Path -LiteralPath (Join-Path $here 'dlssnr-amd\dlssnr.bin'))) { Fail 'Incomplete package: dlssnr-amd\dlssnr.bin is missing.' }
if (Test-Path -LiteralPath $manifest) { Say 'Found a previous installation, removing it first.'; Remove-Installed }

Set-Content -LiteralPath $manifest -Value "F $ManifestName" -Encoding UTF8
Put-Tree (Join-Path $here 'dlssnr-amd') 'dlssnr-amd'
Put-File (Join-Path $here 'vkd3d-proton\d3d12.dll') 'd3d12.dll'
Put-File (Join-Path $here 'vkd3d-proton\d3d12core.dll') 'd3d12core.dll'
Put-File (Join-Path $here 'dxvk\d3d11.dll') 'd3d11.dll'
Put-File (Join-Path $here 'dxvk\d3d10core.dll') 'd3d10core.dll'
# A Vulkan loader that never calls DXGI and finds ReShade's layer in the game folder (both routes need it).
Put-File (Join-Path $here 'vulkan\vulkan-1.dll') 'vulkan-1.dll'
$exeName = [System.IO.Path]::GetFileNameWithoutExtension($exe)
Record-Logs @('dlssnr-amd.log', 'dlssnr-amd-crash.dmp', "${exeName}_dxgi.log", "${exeName}_d3d11.log", "${exeName}_d3d9.log",
              'vkd3d-proton.cache', 'vkd3d-proton.cache.write')

if ($Route -eq 'optiscaler') {
    foreach ($item in Get-ChildItem -LiteralPath (Join-Path $here 'optiscaler\game')) {
        if ($item.Name -eq 'OptiScaler.dll') { continue }
        if ($item.PSIsContainer) { Put-Tree $item.FullName $item.Name } else { Put-File $item.FullName $item.Name }
    }
    # OptiScaler takes the name dxgi.dll and loads the "original" DXGI as dxgi-original.dll. That is our
    # split DLL: the game's calls go to DXVK's dxgi (dxgi-dxvk.dll), the graphics driver's own presentation calls to the system DXGI.
    Put-File (Join-Path $here 'optiscaler\game\OptiScaler.dll') 'dxgi.dll'
    Put-File (Join-Path $here 'dxvk\dxgi.dll') 'dxgi-dxvk.dll'
    foreach ($f in 'dlssnr_core.dll', 'nvngx.dll_dlssnr.dll', 'nvngx_dlssnr.dll', 'dxgi-original.dll') { Put-File (Join-Path $here "optiscaler\$f") $f }
    Record-Logs @('OptiScaler.log')

    $ini = Join-Path $game 'OptiScaler.ini'
    $wanted = [ordered]@{
        'DlssNr|Enabled'               = 'true'
        'Libraries|NvngxPath'          = (Join-Path $game 'dlssnr_core.dll')
        # Auto picks FSR4 on RX 9000, which goes through the AMD driver's own D3D12 extension and does not work on vkd3d-proton.
        'Upscalers|Dx12Upscaler'       = 'xess'
        # With DXVK, overlays such as Steam's load while DXVK creates its Vulkan instance; OptiScaler then
        # calls DXVK's CreateDXGIFactory again, DXVK's instance lock is not reentrant, and the game hangs (7 Days to Die).
        # So this route blocks overlays (OptiScaler's own option); there is no Steam overlay in game.
        'Hotfix|DisableOverlays'       = 'true'
        # OptiScaler's own log is on while this is experimental (Debug level shows the names of blocked overlays).
        'Log|LogToFile'                = 'true'
        'Log|LogLevel'                 = '1'
    }
    $lines = [System.IO.File]::ReadAllLines($ini)
    $section = $null; $seen = @{}
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -match '^\s*\[([^\]]+)\]') { $section = $Matches[1]; continue }
        if ($section -and $lines[$i] -match '^\s*([A-Za-z0-9_]+)\s*=') {
            $key = "$section|$($Matches[1])"
            if ($wanted.Contains($key) -and -not $seen.ContainsKey($key)) {
                $lines[$i] = "$($Matches[1])=$($wanted[$key])"; $seen[$key] = 1
            }
        }
    }
    [System.IO.File]::WriteAllLines($ini, $lines)
    foreach ($key in $wanted.Keys) { if (-not $seen.ContainsKey($key)) { Say "Note: $key not found in OptiScaler.ini" } }
} else {
    # Other ReShade add-ons in the game folder (installed earlier) would load too; move them to the backup folder.
    foreach ($item in Get-ChildItem -LiteralPath $game -File | Where-Object { $_.Extension -in '.addon', '.addon64' }) {
        if ($item.Name -ne 'dlssnr_amd.addon64') { Save-Existing $item.Name; Say "Moved another ReShade add-on aside: $($item.Name)" }
    }
    Put-File (Join-Path $here 'dxvk\dxgi.dll') 'dxgi.dll'
    Put-File (Join-Path $here 'dxvk\d3d9.dll') 'd3d9.dll'
    foreach ($item in Get-ChildItem -LiteralPath (Join-Path $here 'reshade')) {
        if ($item.Name -eq 'ReShadePreset-d3d9.ini') { continue }
        if ($item.PSIsContainer) { Put-Tree $item.FullName $item.Name } else { Put-File $item.FullName $item.Name }
    }
    if ($Route -eq 'dx9') {
        Copy-Item -LiteralPath (Join-Path $here 'reshade\ReShadePreset-d3d9.ini') -Destination (Join-Path $game 'ReShadePreset.ini') -Force
    }
    Record-Logs @('ReShade.log', 'dlssnr-amd.ini')
}

Say ''
Say "Installed into: $game"
if ($Route -eq 'optiscaler') {
    Say 'In the game, turn on DLSS (or FSR / XeSS) in the graphics settings. Insert opens the OptiScaler menu;'
    Say 'the NR settings are on the DLSS Neural Rendering page.'
} else {
    Say 'In the game, Home opens ReShade; the settings are on the Add-ons page, or edit dlssnr-amd.ini in the game folder.'
}
Say 'The first time in game the network has to compile; it starts after about half a minute.'
Say 'Uninstall: run install.bat again and choose 4.'
Finish 0
