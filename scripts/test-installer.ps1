param(
  [string]$BuildDir = 'build/phase2/Release',
  [string]$IsccPath = 'build/tools/inno/ISCC.exe'
)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$smokeRoot = [IO.Path]::GetFullPath((Join-Path $projectDir 'build/installer-smoke'))
if (!$smokeRoot.StartsWith($projectDir + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Smoke path must be in this workspace.' }
$appDir = Join-Path $smokeRoot 'app'
$outputDir = Join-Path $smokeRoot 'output'
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\AIAssistant-Phase2Smoke_is1'
if (Test-Path -LiteralPath $uninstallKey) { throw 'A previous smoke install still exists. Uninstall it before rerunning.' }
$oldRun = (Get-ItemProperty -LiteralPath $runKey -Name AIAssistant -ErrorAction SilentlyContinue).AIAssistant
$expectedRun = '"' + (Join-Path $appDir 'AIAssistant.exe') + '" --background'
function InvokeSetup([string]$File, [string[]]$Arguments) {
  $process = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -Wait -PassThru
  if ($process.ExitCode -ne 0) { throw "Setup failed with exit code $($process.ExitCode)." }
}
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$compiler = if ([IO.Path]::IsPathRooted($IsccPath)) { $IsccPath } else { Join-Path $projectDir $IsccPath }
$binaryDir = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $projectDir $BuildDir }
& $compiler "/DBuildDir=$binaryDir" "/DOutputDir=$outputDir" '/DProductId=AIAssistant-Phase2Smoke' "/DWebViewBootstrapper=$projectDir/build/installer-deps/MicrosoftEdgeWebview2Setup.exe" (Join-Path $projectDir 'installer/AIAssistant.iss') | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Smoke installer compilation failed.' }
$setup = Join-Path $outputDir 'AIAssistant-0.1.0-Setup.exe'
$uninstall = Join-Path $appDir 'unins000.exe'
try {
  InvokeSetup $setup @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', ('/DIR="' + $appDir + '"'), '/TASKS=autostart', ('/LOG="' + (Join-Path $smokeRoot 'install.log') + '"'))
  foreach ($file in @('AIAssistant.exe', 'ui/bar.html', 'ui/settings.html', 'unins000.exe')) {
    if (!(Test-Path -LiteralPath (Join-Path $appDir $file))) { throw "Missing installed file: $file" }
  }
  if (!(Test-Path -LiteralPath $uninstallKey)) { throw 'No Add/Remove Programs registration.' }
  if ((Get-ItemProperty -LiteralPath $runKey -Name AIAssistant).AIAssistant -ne $expectedRun) { throw 'Autostart was not configured correctly.' }
  Write-Output 'PASS install, bundled UI, uninstall registration and optional autostart'
  InvokeSetup $setup @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', ('/DIR="' + $appDir + '"'), '/TASKS=')
  if ((Get-ItemProperty -LiteralPath $runKey -Name AIAssistant -ErrorAction SilentlyContinue).AIAssistant -eq $expectedRun) { throw 'Autostart was not removed when unchecked.' }
  Write-Output 'PASS reinstall with autostart unchecked'
  InvokeSetup $setup @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', ('/DIR="' + $appDir + '"'), '/TASKS=autostart')
  InvokeSetup $uninstall @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', ('/LOG="' + (Join-Path $smokeRoot 'uninstall.log') + '"'))
  if (Test-Path -LiteralPath (Join-Path $appDir 'AIAssistant.exe')) { throw 'Uninstall left the executable.' }
  if (Test-Path -LiteralPath $uninstallKey) { throw 'Uninstall left its registration.' }
  if ((Get-ItemProperty -LiteralPath $runKey -Name AIAssistant -ErrorAction SilentlyContinue).AIAssistant -eq $expectedRun) { throw 'Uninstall left autostart enabled.' }
  Write-Output 'PASS uninstall removes app, registration and its autostart value'
} finally {
  if (Test-Path -LiteralPath $uninstall) { InvokeSetup $uninstall @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') }
  if ($null -ne $oldRun) { Set-ItemProperty -LiteralPath $runKey -Name AIAssistant -Value $oldRun }
  else {
    $current = (Get-ItemProperty -LiteralPath $runKey -Name AIAssistant -ErrorAction SilentlyContinue).AIAssistant
    if ($current -eq $expectedRun) { Remove-ItemProperty -LiteralPath $runKey -Name AIAssistant }
  }
}
