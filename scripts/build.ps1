[CmdletBinding()]
param(
    [ValidateSet('debug', 'releasedbg', 'release')]
    [string]$Configuration = 'releasedbg',
    [ValidateSet('dear-main', 'wushen-main')]
    [string]$CommonLibF4Profile,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot '..\..'))
$buildEntry = Join-Path $workspaceRoot 'tools\build\build-cpp.ps1'

$arguments = @{
    ProjectRoot = $projectRoot
    Target = 'ItemIntegrationFramework'
    Configuration = $Configuration
}
if ($PSBoundParameters.ContainsKey('CommonLibF4Profile')) {
    $arguments.CommonLibF4Profile = $CommonLibF4Profile
}
if ($ValidateOnly) {
    $arguments.ValidateOnly = $true
}

& $buildEntry @arguments
