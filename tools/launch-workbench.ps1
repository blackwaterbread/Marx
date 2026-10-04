<#
.SYNOPSIS
Starts Arma Reforger Workbench with the Marx addons, without the launcher.

.DESCRIPTION
Opens a Marx project (default: Marx_Example, which loads Marx_Shop and Marx_Core) with the Marx addon
folders and the game's addon folder registered through -addonsDir. Workbench and the game are looked up
in the Steam library folders listed in Steam's libraryfolders.vdf, unless given explicitly.

.PARAMETER Project
Addon folder under addons/ to open. Default: Example.

.PARAMETER AutoCloseTests
Passes -mrxTestsAutoClose: the Workbench-only Marx test harness leaves Play mode after its run.

.PARAMETER NoScriptAuthorizeAll
Do not pass -scriptAuthorizeAll (which suppresses the "Script Authorization Required" prompt).

.PARAMETER WorkbenchExe
Path to ArmaReforgerWorkbenchSteamDiag.exe. Default: found in the Steam libraries.

.PARAMETER GameDir
Arma Reforger install folder. Default: found in the Steam libraries.

.PARAMETER LogsDir
Folder for this session's logs. Default: a new logs_<timestamp> folder in the Workbench logs folder
(Documents\My Games\ArmaReforgerWorkbench\logs). Without -logsDir, a Workbench started outside the
launcher appends to console.log in the game folder.

.PARAMETER DryRun
Print the command line without starting Workbench.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File tools/launch-workbench.ps1 -AutoCloseTests
#>
param(
	[string]$Project = "Example",
	[switch]$AutoCloseTests,
	[switch]$NoScriptAuthorizeAll,
	[string]$WorkbenchExe,
	[string]$GameDir,
	[string]$LogsDir,
	[switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Get-SteamLibraries
{
	$steamRoot = $null
	try
	{
		$steamRoot = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -Name SteamPath -ErrorAction Stop).SteamPath
	}
	catch
	{
		$steamRoot = "C:\Program Files (x86)\Steam"
	}

	$libraries = @($steamRoot)
	$vdf = Join-Path $steamRoot "steamapps\libraryfolders.vdf"
	if (Test-Path $vdf)
	{
		foreach ($match in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"'))
		{
			$libraries += $match.Groups[1].Value.Replace("\\", "\")
		}
	}

	return $libraries | Select-Object -Unique
}

function Find-InSteamLibraries([string]$relativePath)
{
	foreach ($library in Get-SteamLibraries)
	{
		$candidate = Join-Path $library $relativePath
		if (Test-Path $candidate)
		{
			return $candidate
		}
	}

	return $null
}

if (-not $WorkbenchExe)
{
	$WorkbenchExe = Find-InSteamLibraries "steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe"
}

if (-not $GameDir)
{
	$GameDir = Find-InSteamLibraries "steamapps\common\Arma Reforger"
}

if (-not $WorkbenchExe -or -not (Test-Path $WorkbenchExe))
{
	throw "Workbench not found. Pass -WorkbenchExe."
}

if (-not $GameDir -or -not (Test-Path (Join-Path $GameDir "addons")))
{
	throw "Arma Reforger not found. Pass -GameDir."
}

$addonsRoot = Join-Path (Split-Path $PSScriptRoot -Parent) "addons"
$gproj = Join-Path $addonsRoot "$Project\addon.gproj"
if (-not (Test-Path $gproj))
{
	throw "Project not found: $gproj"
}

# Each Marx addon folder plus the game's addons folder, as one comma-separated list.
$addonDirs = @(Get-ChildItem -Path $addonsRoot -Directory | Where-Object { Test-Path (Join-Path $_.FullName "addon.gproj") } | ForEach-Object { $_.FullName })
$addonDirs += (Join-Path $GameDir "addons")

if (-not $LogsDir)
{
	$logsRoot = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\ArmaReforgerWorkbench\logs"
	$LogsDir = Join-Path $logsRoot ("logs_" + (Get-Date -Format "yyyy-MM-dd_HH-mm-ss"))
}

$arguments = @("-gproj", "`"$gproj`"", "-addonsDir", "`"$($addonDirs -join ',')`"", "-logsDir", "`"$LogsDir`"")
if (-not $NoScriptAuthorizeAll)
{
	$arguments += "-scriptAuthorizeAll"
}

if ($AutoCloseTests)
{
	$arguments += "-mrxTestsAutoClose"
}

Write-Host "Workbench: $WorkbenchExe"
Write-Host "Working directory: $GameDir"
Write-Host "Logs: $LogsDir"
Write-Host "Arguments: $($arguments -join ' ')"
if ($DryRun)
{
	return
}

if (Get-Process -Name "ArmaReforgerWorkbenchSteamDiag" -ErrorAction SilentlyContinue)
{
	throw "Workbench is already running. Close it first."
}

New-Item -ItemType Directory -Force -Path $LogsDir | Out-Null
$process = Start-Process -FilePath $WorkbenchExe -ArgumentList $arguments -WorkingDirectory $GameDir -PassThru
Write-Host "Started process $($process.Id)"
