param(
    [string]$QtVersion = "6.9.0",
    [string]$QtArch = "win64_msvc2022_64"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Info([string]$Message) {
    Write-Host "==> $Message"
}

function Ensure-Scoop {
    if (Get-Command scoop -ErrorAction SilentlyContinue) {
        return
    }

    Write-Info "Scoop not found; installing it"
    Set-ExecutionPolicy RemoteSigned -Scope CurrentUser -Force
    Invoke-RestMethod https://get.scoop.sh | Invoke-Expression
}

function Install-ScoopPackage {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names
    )

    foreach ($name in $Names) {
        try {
            Write-Info "Installing Scoop package: $name"
            & scoop install $name
            return $true
        } catch {
            Write-Host "warning: Scoop package '$name' was not installed ($($_.Exception.Message))"
        }
    }

    return $false
}

function Set-UserEnv {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    [Environment]::SetEnvironmentVariable($Name, $Value, 'User')
    Set-Item -Path ("Env:{0}" -f $Name) -Value $Value
}

function Prepend-UserPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathEntry
    )

    $current = [Environment]::GetEnvironmentVariable('Path', 'User')
    $segments = @()
    if ($current) {
        $segments = $current -split ';' | Where-Object { $_ -and $_.Trim().Length -gt 0 }
    }

    if ($segments -notcontains $PathEntry) {
        $updated = @($PathEntry) + $segments
        [Environment]::SetEnvironmentVariable('Path', ($updated -join ';'), 'User')
        Set-Item -Path 'Env:Path' -Value "$PathEntry;$env:Path"
    }
}

Ensure-Scoop

if (-not (scoop bucket list | Select-String -SimpleMatch 'extras')) {
    Write-Info "Adding Scoop extras bucket"
    & scoop bucket add extras
}

Write-Info "Installing core build tools"
& scoop install git cmake ninja python pkgconf 7zip

Write-Info "Installing Qt through aqtinstall"
& python -m pip install --user --upgrade pip aqtinstall
$qtRoot = Join-Path $env:LOCALAPPDATA 'Qt'
& python -m aqt install-qt windows desktop $QtVersion $QtArch -O $qtRoot

$qtPrefix = Join-Path $qtRoot "$QtVersion\$QtArch"
Set-UserEnv -Name 'QT_PREFIX' -Value $qtPrefix
Prepend-UserPath -PathEntry (Join-Path $qtPrefix 'bin')

Write-Info "Installing image libraries"
$pkgConfigPaths = @()
foreach ($pkg in @('libheif', 'libraw', 'jpeg-turbo')) {
    if (Install-ScoopPackage -Names @($pkg)) {
        try {
            $prefix = & scoop prefix $pkg
            foreach ($candidate in @('lib/pkgconfig', 'lib64/pkgconfig', 'share/pkgconfig')) {
                $path = Join-Path $prefix $candidate
                if (Test-Path $path) {
                    $pkgConfigPaths += $path
                }
            }
        } catch {
            Write-Host "warning: could not determine Scoop prefix for $pkg"
        }
    } else {
        Write-Host "warning: package '$pkg' was not available from the current Scoop buckets."
    }
}

if ($pkgConfigPaths.Count -gt 0) {
    $currentPkgConfig = [Environment]::GetEnvironmentVariable('PKG_CONFIG_PATH', 'User')
    $merged = @($pkgConfigPaths + ($currentPkgConfig -split ';' | Where-Object { $_ -and $_.Trim().Length -gt 0 }))
    [Environment]::SetEnvironmentVariable('PKG_CONFIG_PATH', ($merged | Select-Object -Unique) -join ';', 'User')
    $env:PKG_CONFIG_PATH = [Environment]::GetEnvironmentVariable('PKG_CONFIG_PATH', 'User')
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Write-Host ""
    Write-Host "warning: MSVC Build Tools are not detected in the current shell."
    Write-Host "Install Visual Studio 2022 Build Tools with the 'Desktop development with C++' workload,"
    Write-Host "then open a 'x64 Native Tools Command Prompt for VS 2022' or VS Code from that environment."
}

Write-Host ""
Write-Info "Development environment setup complete"
Write-Host "Qt prefix: $qtPrefix"
Write-Host "Run the build with: scripts\build.bat"
