$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$settings = Join-Path $root 'ui\FlowWheel.UI\AppSettings.cs'
$program = Join-Path $root 'ui\FlowWheel.UI\Program.cs'
$form = Join-Path $root 'ui\FlowWheel.UI\SettingsForm.cs'
foreach ($path in @($settings, $program, $form)) {
    if (-not (Test-Path $path)) { throw "Missing required source: $path" }
}
if ((Get-Content $settings -Raw) -notmatch 'SettingsStore') { throw 'SettingsStore contract missing' }
if ((Get-Content $program -Raw) -notmatch 'NotifyIcon') { throw 'Tray host contract missing' }
if ((Get-Content $program -Raw) -notmatch 'CurrentUser.CreateSubKey') { throw 'Startup registration contract missing' }
Write-Host 'FlowWheel source smoke checks passed.'
