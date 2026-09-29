param(
    [ValidateSet('win-x64', 'win-arm64')]
    [string]$Runtime = 'win-x64',
    [switch]$Installer
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'ui\FlowWheel.UI\FlowWheel.UI.csproj'
$publish = Join-Path $root "artifacts\$Runtime"

New-Item -ItemType Directory -Force -Path $publish | Out-Null
dotnet publish $project -c Release -r $Runtime --self-contained true `
    -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true `
    -p:EnableCompressionInSingleFile=true -o $publish

$exe = Join-Path $publish 'FlowWheel.exe'
if (-not (Test-Path $exe)) { throw "Publish completed without FlowWheel.exe" }
Write-Host "FlowWheel published to $exe"

if ($Installer) {
    $installerDir = Join-Path $root 'artifacts\installer'
    New-Item -ItemType Directory -Force -Path $installerDir | Out-Null
    $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    if (-not (Test-Path $csc)) { $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe' }
    if (-not (Test-Path $csc)) { throw 'The .NET Framework C# compiler (csc.exe) was not found.' }
    $setup = Join-Path $installerDir 'FlowWheel-Setup.exe'
    $cscArgs = @(
        '/nologo', '/target:winexe', '/platform:anycpu', '/optimize+',
        "/out:$setup", "/resource:$exe,FLOWWHEEL_APP",
        '/r:System.dll', '/r:System.Core.dll', '/r:System.Drawing.dll',
        '/r:System.Windows.Forms.dll', '/r:Microsoft.CSharp.dll',
        (Join-Path $root 'installer\FlowWheelInstaller.cs')
    )
    & $csc @cscArgs
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $setup)) { throw 'Installer compilation failed.' }
    Write-Host "Installer published to $setup"
}
