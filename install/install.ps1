[CmdletBinding()]
param(
    [Alias('n')][switch]$Minimal,
    [Alias('v')][string]$Version,
    [Alias('p')][string]$Package,
    [Alias('h')][switch]$Help
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$repo = 'mochapulse/tinto-transcribe-tool'

if ($Help) {
    Write-Output @'
Usage: install.ps1 [-Minimal] [-Version VERSION] [-Package ARCHIVE]
  -n, -Minimal          Skip the model downloads.
  -v, -Version VERSION  Install a specific tag instead of the latest release.
  -p, -Package ARCHIVE  Install a local ZIP with its .sha256 file alongside.
'@
    return
}
if ($Version -and -not $Version.StartsWith('v')) { $Version = "v$Version" }

$os = Get-CimInstance Win32_OperatingSystem
$architecture = if ($env:PROCESSOR_ARCHITEW6432) {
    $env:PROCESSOR_ARCHITEW6432
} else {
    $env:PROCESSOR_ARCHITECTURE
}
if ([int]$os.BuildNumber -lt 22000 -or [int]$os.ProductType -ne 1 -or $architecture -ne 'AMD64') {
    throw 'tinto requires Windows 11 x64.'
}
Write-Output "install: detected Windows 11 x64 (build $($os.BuildNumber))"

$system = Join-Path $env:WINDIR 'System32'
$runtime = @('vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll', 'vcomp140.dll')
$missing = @($runtime | Where-Object { -not (Test-Path (Join-Path $system $_) -PathType Leaf) })
if ($missing.Count) {
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) { throw 'Install the Microsoft Visual C++ x64 Redistributable or enable winget.' }
    & $winget.Source install --id 'Microsoft.VCRedist.2015+.x64' --exact --silent --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) { throw 'Visual C++ Redistributable installation failed.' }
    foreach ($dll in $missing) {
        if (-not (Test-Path (Join-Path $system $dll) -PathType Leaf)) {
            throw "Required runtime still missing: $dll"
        }
    }
}
if (-not (Test-Path (Join-Path $system 'curl.exe') -PathType Leaf)) {
    throw 'Windows curl.exe is required by tinto init.'
}

$destination = Join-Path $env:LOCALAPPDATA 'Programs\tinto'
$exe = Join-Path $destination 'tinto.exe'
if (Test-Path -LiteralPath $exe) {
    & $exe status | Out-Null
    if ($LASTEXITCODE -eq 0) { throw 'Run tinto stop before updating an active installation.' }
}
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("tinto-install-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
    if (-not $Package) {
        if (-not $Version) {
            $Version = (Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest").tag_name
        }
        if (-not $Version.StartsWith('v')) { $Version = "v$Version" }
        if ($Version -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+$') { throw 'Invalid or unavailable release version.' }
        $name = "tinto-$Version-windows-x86_64.zip"
        $url = "https://github.com/$repo/releases/download/$Version/$name"
        $Package = Join-Path $tmp $name
        Invoke-WebRequest -Uri $url -OutFile $Package -UseBasicParsing
        Invoke-WebRequest -Uri "$url.sha256" -OutFile "$Package.sha256" -UseBasicParsing
    }
    $Package = (Resolve-Path -LiteralPath $Package).Path
    $expected = (Get-Content -LiteralPath "$Package.sha256" -Raw).Trim() -split '\s+' | Select-Object -First 1
    if ($expected -notmatch '^[a-fA-F0-9]{64}$' -or
        (Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash -ne $expected) {
        throw 'Package checksum did not match.'
    }
    $bundle = Join-Path $tmp 'bundle'
    Expand-Archive -LiteralPath $Package -DestinationPath $bundle
    $candidate = Join-Path $bundle 'tinto.exe'
    $installedVersion = & $candidate -v
    if ($LASTEXITCODE -ne 0 -or ($Version -and "v$installedVersion" -ne $Version)) {
        throw 'The packaged executable did not run or has a different version.'
    }

    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $bundle) {
        Copy-Item -LiteralPath $item.FullName -Destination $destination -Recurse -Force
    }
    if ($Minimal) {
        & $exe init -n
        if ($LASTEXITCODE -ne 0) { throw 'tinto init failed.' }
    } else {
        & $exe init
        if ($LASTEXITCODE -ne 0) { throw 'tinto init failed; rerun it to complete model setup.' }
        & $exe health
        if ($LASTEXITCODE -ne 0) { throw 'tinto health reported incomplete setup.' }
    }
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    if (($userPath -split ';') -notcontains $destination) {
        $newPath = (@($destination, $userPath) | Where-Object { $_ }) -join ';'
        [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
    }
    if (($env:Path -split ';') -notcontains $destination) { $env:Path = "$destination;$env:Path" }
    Write-Output "install: tinto $installedVersion installed at $exe"
    Write-Output 'Run tinto --help for examples.'
} finally {
    Remove-Item -LiteralPath $tmp -Recurse -Force
}
