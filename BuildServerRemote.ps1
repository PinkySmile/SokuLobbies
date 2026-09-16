<#
.SYNOPSIS
	Builds the committed SokuLobbies server source on the configured Linux host.

.DESCRIPTION
	Archives a Git commit locally, uploads it over SCP, builds SokuLobbiesServer in
	a new directory under /root/backup, verifies the compile definitions, and
	prints the artifact path and SHA-256. It never replaces a running server.

.EXAMPLE
	.\BuildServerRemote.ps1

.EXAMPLE
	.\BuildServerRemote.ps1 -Ref origin/master -BuildName SokuLobbies-master-test
#>
[CmdletBinding()]
param(
	[string]$Server = "root@43.136.23.115",
	[string]$KeyPath = "$env:USERPROFILE\Downloads\id_rsa",
	[string]$Ref = "HEAD",
	[string]$RemoteRoot = "/root/backup",
	[string]$BuildName = "",
	[ValidateRange(1, 32)]
	[int]$Jobs = 2
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repository = $PSScriptRoot
$key = (Resolve-Path -LiteralPath $KeyPath).Path
$commit = (& git -C $repository rev-parse "$Ref^{commit}").Trim()
if ($LASTEXITCODE -ne 0 -or !$commit) {
	throw "Cannot resolve Git ref '$Ref'."
}
$shortCommit = $commit.Substring(0, 8)
if (!$BuildName) {
	$BuildName = "SokuLobbies-server-$shortCommit-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
}
if ($BuildName -notmatch '^[A-Za-z0-9._-]+$') {
	throw "BuildName may contain only letters, numbers, dots, underscores, and hyphens."
}
if ($RemoteRoot -notmatch '^/[A-Za-z0-9._/-]+$') {
	throw "RemoteRoot must be an absolute path containing only safe path characters."
}

$remoteSource = "$RemoteRoot/$BuildName"
$remoteArchive = "$RemoteRoot/$BuildName.tar.gz"
$remoteBuild = "$remoteSource/build"
$remoteArtifact = "$remoteBuild/SokuLobbiesServer"
$localArchive = Join-Path ([IO.Path]::GetTempPath()) "$BuildName.tar.gz"
$sshOptions = @('-i', $key, '-o', 'IdentitiesOnly=yes', '-o', 'BatchMode=yes')

try {
	Write-Host "Archiving commit $commit..."
	& git -C $repository archive --format=tar.gz --output=$localArchive $commit
	if ($LASTEXITCODE -ne 0) {
		throw "git archive failed."
	}

	$prepare = "set -e; test ! -e '$remoteSource'; test ! -e '$remoteArchive'; mkdir -p '$RemoteRoot'"
	& ssh @sshOptions $Server $prepare
	if ($LASTEXITCODE -ne 0) {
		throw "Remote build path already exists or could not be prepared: $remoteSource"
	}

	Write-Host "Uploading source archive..."
	& scp @sshOptions $localArchive "${Server}:$remoteArchive"
	if ($LASTEXITCODE -ne 0) {
		throw "Source upload failed."
	}

	Write-Host "Building SokuLobbiesServer..."
	$build = @"
set -e
mkdir -p '$remoteSource'
tar -xzf '$remoteArchive' -C '$remoteSource'
rm -f '$remoteArchive'
cmake -S '$remoteSource' -B '$remoteBuild' -DCMAKE_BUILD_TYPE=Release
cmake --build '$remoteBuild' --target SokuLobbiesServer -j$Jobs
test -x '$remoteArtifact'
grep -E '_LOBBYNOLOG|MAIN_SERVER_HOST' '$remoteBuild/CMakeFiles/SokuLobbiesServer.dir/flags.make'
ls -lh '$remoteArtifact'
sha256sum '$remoteArtifact'
"@
	& ssh @sshOptions $Server $build
	if ($LASTEXITCODE -ne 0) {
		throw "Remote server build failed. Sources and build output were kept at $remoteSource for inspection."
	}

	Write-Host "Build completed: $remoteArtifact"
} finally {
	if (Test-Path -LiteralPath $localArchive) {
		Remove-Item -LiteralPath $localArchive -Force
	}
}
