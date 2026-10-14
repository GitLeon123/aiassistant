param(
  [string]$BuildDir = 'build/msvc-x64/Release',
  [string]$OutputDir = 'build/installer',
  [string]$Version = '0.1.0',
  [string]$IsccPath = ''
)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
function ProjectPath([string]$Path) {
  if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
  return [IO.Path]::GetFullPath((Join-Path $projectDir $Path))
}
$binaryDir = ProjectPath $BuildDir
$destinationDir = ProjectPath $OutputDir
foreach ($file in @('AIAssistant.exe', 'ui/bar.html', 'ui/settings.html')) {
  if (!(Test-Path -LiteralPath (Join-Path $binaryDir $file))) { throw "Build the Release app and UI first. Missing: $file" }
}
if (!$IsccPath) {
  $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
  if ($command) { $IsccPath = $command.Source }
  else {
    $IsccPath = @(
      (Join-Path $projectDir 'build/tools/inno/ISCC.exe'),
      "${env:ProgramFiles(x86)}/Inno Setup 6/ISCC.exe",
      "${env:ProgramFiles(x86)}/Inno Setup 7/ISCC.exe",
      "$env:LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe",
      "$env:LOCALAPPDATA/Programs/Inno Setup 7/ISCC.exe"
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
  }
}
if (!$IsccPath -or !(Test-Path -LiteralPath $IsccPath)) { throw 'Install Inno Setup 6.3+ from https://jrsoftware.org/isdl.php, or pass -IsccPath.' }
$dependencyDir = Join-Path $projectDir 'build/installer-deps'
New-Item -ItemType Directory -Path $dependencyDir,$destinationDir -Force | Out-Null
$bootstrapper = Join-Path $dependencyDir 'MicrosoftEdgeWebview2Setup.exe'
if (!(Test-Path -LiteralPath $bootstrapper)) {
  Invoke-WebRequest 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile $bootstrapper
}
$signature = Get-AuthenticodeSignature -LiteralPath $bootstrapper
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
  throw 'The WebView2 bootstrapper must have a valid Microsoft signature. Remove the cached download and retry.'
}
& $IsccPath "/DBuildDir=$binaryDir" "/DOutputDir=$destinationDir" "/DAppVersion=$Version" "/DWebViewBootstrapper=$bootstrapper" (Join-Path $projectDir 'installer/AIAssistant.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed ($LASTEXITCODE)." }
$setupFile = Join-Path $destinationDir "AIAssistant-$Version-Setup.exe"
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $setupFile).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$setupFile.sha256" -Value "$hash  $([IO.Path]::GetFileName($setupFile))" -Encoding ascii
Write-Output "Installer: $setupFile"
