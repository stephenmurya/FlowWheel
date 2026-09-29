[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string]$Architecture = 'x64',
    [switch]$Installer
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$native = Join-Path $root 'native'
$build = Join-Path $native 'build'
$output = Join-Path $root 'artifacts\native'

cmake -S $native -B $build -A $Architecture
if ($LASTEXITCODE -ne 0) { throw 'Native CMake configuration failed.' }

cmake --build $build --config Release --clean-first
if ($LASTEXITCODE -ne 0) { throw 'Native C++ build failed.' }

New-Item -ItemType Directory -Force -Path $output | Out-Null
$builtExe = Join-Path $build 'Release\FlowWheel.exe'
$portable = Join-Path $output 'FlowWheel.exe'
Copy-Item -LiteralPath $builtExe -Destination $portable -Force
Write-Host "Native portable executable: $portable"

if ($Installer) {
    $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    if (-not (Test-Path -LiteralPath $csc)) {
        $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
    }
    if (-not (Test-Path -LiteralPath $csc)) {
        throw 'The .NET Framework C# compiler (csc.exe) was not found.'
    }

    $setup = Join-Path $output 'FlowWheel-Setup.exe'
    $source = Join-Path $root 'installer\FlowWheelInstaller.cs'
    $references = @('System.dll', 'System.Drawing.dll', 'System.Windows.Forms.dll')
    $arguments = @('/nologo', '/target:winexe', "/out:$setup", "/resource:$portable,FLOWWHEEL_APP")
    $arguments += $references | ForEach-Object { "/reference:$_" }
    $arguments += $source
    & $csc $arguments
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $setup)) {
        throw 'Native installer compilation failed.'
    }
    Write-Host "Native installer: $setup"
}
