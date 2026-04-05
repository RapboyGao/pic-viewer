param(
    [string]$VcpkgTriplet = "x64-windows"
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

function Ensure-Vcpkg {
    $scriptRoot = Split-Path -Parent $PSScriptRoot
    $vcpkgRoot = Join-Path $scriptRoot '.deps\vcpkg'

    if ((-not (Test-Path (Join-Path $vcpkgRoot '.git'))) -or (-not (Test-Path (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat')))) {
        if (Test-Path $vcpkgRoot) {
            Write-Info "Removing incomplete vcpkg directory"
            Remove-Item -Recurse -Force $vcpkgRoot
        }

        Write-Info "Cloning vcpkg"
        & git clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkgRoot | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "git clone for vcpkg failed with exit code $LASTEXITCODE"
        }
    }

    $lockFile = Join-Path $vcpkgRoot '.vcpkg-root'
    $vcpkgRunning = Get-Process vcpkg -ErrorAction SilentlyContinue
    if (-not $vcpkgRunning -and (Test-Path $lockFile)) {
        Write-Info "Removing stale vcpkg lock file"
        Remove-Item -Force $lockFile
    }

    $bootstrap = Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat'
    if (-not (Test-Path (Join-Path $vcpkgRoot 'vcpkg.exe'))) {
        Write-Info "Bootstrapping vcpkg"
        & $bootstrap -disableMetrics | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg bootstrap failed with exit code $LASTEXITCODE"
        }
    }

    if (-not (Test-Path (Join-Path $vcpkgRoot 'vcpkg.exe'))) {
        throw "vcpkg bootstrap failed"
    }

    return $vcpkgRoot
}

function Add-PkgConfigPathFromRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $pkgConfigPaths = @()
    foreach ($candidate in @(
        (Join-Path $Root 'lib/pkgconfig'),
        (Join-Path $Root 'lib64/pkgconfig'),
        (Join-Path $Root 'share/pkgconfig')
    )) {
        if (Test-Path $candidate) {
            $pkgConfigPaths += $candidate
        }
    }

    if ($pkgConfigPaths.Count -eq 0) {
        return
    }

    $currentPkgConfig = [Environment]::GetEnvironmentVariable('PKG_CONFIG_PATH', 'User')
    $merged = @($pkgConfigPaths + ($currentPkgConfig -split ';' | Where-Object { $_ -and $_.Trim().Length -gt 0 }))
    $unique = $merged | Select-Object -Unique
    [Environment]::SetEnvironmentVariable('PKG_CONFIG_PATH', ($unique -join ';'), 'User')
    $env:PKG_CONFIG_PATH = [Environment]::GetEnvironmentVariable('PKG_CONFIG_PATH', 'User')
}

function Ensure-VisualStudioBuildTools {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "Visual Studio Build Tools are required but winget is not available"
    }

    Write-Info "Installing Visual Studio Build Tools 2022 with the C++ workload"
    $args = @(
        'install',
        '--id', 'Microsoft.VisualStudio.2022.BuildTools',
        '--override', '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended',
        '--accept-package-agreements',
        '--accept-source-agreements',
        '--disable-interactivity'
    )
    & winget @args | Out-Null
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    $clFound = Get-Command cl.exe -ErrorAction SilentlyContinue
    $vsFound = $false
    if (Test-Path $vswhere) {
        $vsFound = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }

    if (-not $clFound -and -not $vsFound) {
        throw "winget install for Visual Studio Build Tools failed with exit code $LASTEXITCODE"
    }

    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installPath) {
            return $installPath
        }
    }

    $fallbackPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
    if (Test-Path $fallbackPath) {
        return $fallbackPath
    }

    return $null
}

Ensure-Scoop

if (-not (scoop bucket list | Select-String -SimpleMatch 'extras')) {
    Write-Info "Adding Scoop extras bucket"
    & scoop bucket add extras
}

Write-Info "Installing core build tools"
& scoop install git cmake ninja python pkg-config 7zip

Write-Info "Installing image libraries and Qt through vcpkg"
$repoRoot = Split-Path -Parent $PSScriptRoot
$vcpkgRoot = Ensure-Vcpkg
Prepend-UserPath -PathEntry $vcpkgRoot

$installedRoot = Join-Path $vcpkgRoot "installed\$VcpkgTriplet"
$vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
$vsInstallPath = Ensure-VisualStudioBuildTools
$installArgs = @('install', '--triplet', $VcpkgTriplet, '--vcpkg-root', $vcpkgRoot)
if ($vsInstallPath) {
    $vsDevCmd = Join-Path $vsInstallPath 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path $vsDevCmd) {
        $vcpkgCommand = "`"$vcpkgExe`" $($installArgs -join ' ')"
        & cmd.exe /c "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && cd /d `"$repoRoot`" && $vcpkgCommand"
    } else {
        Push-Location $repoRoot
        try {
            & $vcpkgExe @installArgs
        } finally {
            Pop-Location
        }
    }
} else {
    Push-Location $repoRoot
    try {
        & $vcpkgExe @installArgs
    } finally {
        Pop-Location
    }
}

if ($LASTEXITCODE -ne 0) {
    throw "vcpkg package install failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $installedRoot)) {
    throw "Expected vcpkg install root not found: $installedRoot"
}

$qtPrefix = $installedRoot
Set-UserEnv -Name 'QT_PREFIX' -Value $qtPrefix
Prepend-UserPath -PathEntry (Join-Path $qtPrefix 'bin')
Add-PkgConfigPathFromRoot -Root $qtPrefix

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Write-Host ""
    Write-Host "warning: MSVC Build Tools are not detected in the current shell."
    Write-Host "If later builds fail to locate the compiler, open a 'x64 Native Tools Command Prompt for VS 2022' or"
    Write-Host "launch VS Code from that environment."
}

Write-Host ""
Write-Info "Development environment setup complete"
Write-Host "vcpkg root: $vcpkgRoot"
Write-Host "Qt prefix: $qtPrefix"
Write-Host "Run the build with: scripts\build.bat"
