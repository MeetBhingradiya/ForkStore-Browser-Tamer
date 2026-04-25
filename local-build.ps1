param(
    [string]$Config = "Release",
    [string]$Triplet = "x64-windows-static",
    [string]$Generator = "Ninja",
    [string]$Architecture = "x64",
    [string]$Target = "bt",
    [switch]$UpdateTools,
    [switch]$SkipUnblock,
    [switch]$SkipShortcut,
    [string]$PickerUrl = "https://meetbhingradiya.in/bookmarks/newtab",
    [string]$ShortcutPath = "",
    [string]$CodeSignThumbprint = "",
    [switch]$UseDebugCodeSign,
    [string]$DebugCertSubject = "ForkStore Browser Tamer Debug Signing",
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [switch]$CreateProductionPackage,
    [string]$ProductionOutputDir = ".\dist",
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"

function Ensure-Winget {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "winget is required for automatic setup but was not found on this system."
    }
}

function Refresh-ProcessPath {
    # Pull latest PATH from machine/user scopes so newly installed tools are discoverable immediately.
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"
}

function Test-WingetPackageInstalled {
    param(
        [Parameter(Mandatory = $true)][string]$Id
    )

    Ensure-Winget
    & winget list --id $Id -e --source winget --accept-source-agreements | Out-Null
    return ($LASTEXITCODE -eq 0)
}

function Install-Or-UpgradeWingetPackage {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [string]$ExtraArgs = ""
    )

    Ensure-Winget

    $installCmd = "winget install --id $Id -e --source winget --accept-source-agreements --accept-package-agreements"
    if (-not [string]::IsNullOrWhiteSpace($ExtraArgs)) {
        $installCmd += " --override `"$ExtraArgs`""
    }

    & cmd.exe /c $installCmd

    if ($LASTEXITCODE -ne 0 -and (Test-WingetPackageInstalled -Id $Id)) {
        # winget install often returns non-zero when package is already present.
        Refresh-ProcessPath
        return
    }

    if ($LASTEXITCODE -ne 0 -and $UpdateTools) {
        $upgradeCmd = "winget upgrade --id $Id -e --source winget --accept-source-agreements --accept-package-agreements"
        if (-not [string]::IsNullOrWhiteSpace($ExtraArgs)) {
            $upgradeCmd += " --override `"$ExtraArgs`""
        }

        & cmd.exe /c $upgradeCmd
        if ($LASTEXITCODE -ne 0 -and -not (Test-WingetPackageInstalled -Id $Id)) {
            throw "Failed to install or upgrade package '$Id'."
        }
    }
    elseif ($LASTEXITCODE -ne 0) {
        throw "Failed to install package '$Id'. Re-run with -UpdateTools to attempt upgrade fallback."
    }

    Refresh-ProcessPath
}

function Ensure-CommandInstalled {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$WingetId
    )

    if (-not (Get-Command $Command -ErrorAction SilentlyContinue)) {
        Write-Host "Installing missing tool '$Command' via winget package '$WingetId'..."
        Install-Or-UpgradeWingetPackage -Id $WingetId

        if (-not (Get-Command $Command -ErrorAction SilentlyContinue)) {
            throw "Tool '$Command' is still not available after installing '$WingetId'. Restart terminal and retry."
        }
    }
    elseif ($UpdateTools) {
        Write-Host "Updating tool '$Command' via winget package '$WingetId'..."
        Ensure-Winget
        & winget upgrade --id $WingetId -e --source winget --accept-source-agreements --accept-package-agreements
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Skipping update for '$WingetId' because upgrade was unavailable or failed."
        }

        Refresh-ProcessPath
    }
}

function Ensure-BuildTools {
    $vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        Write-Host "Installing Visual Studio Build Tools with C++ workload..."
        Install-Or-UpgradeWingetPackage -Id "Microsoft.VisualStudio.2022.BuildTools" -ExtraArgs "--wait --passive --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    }

    if (-not (Test-Path $vsDevCmd)) {
        throw "Visual Studio Build Tools installation did not provide VsDevCmd.bat."
    }

    cmd.exe /c "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && where cl >nul"
    if ($LASTEXITCODE -ne 0) {
        throw "MSVC compiler was not detected after Build Tools installation."
    }
}

function Ensure-Vcpkg {
    if (-not $env:VCPKG_ROOT) {
        $defaultVcpkgRoot = "C:\dev\vcpkg"
        Write-Host "VCPKG_ROOT is not set. Using '$defaultVcpkgRoot'."

        if (-not (Test-Path "C:\dev")) {
            New-Item -ItemType Directory -Path "C:\dev" | Out-Null
        }

        $env:VCPKG_ROOT = $defaultVcpkgRoot
        setx VCPKG_ROOT $defaultVcpkgRoot | Out-Null
    }

    if (-not (Test-Path $env:VCPKG_ROOT)) {
        Ensure-CommandInstalled -Command "git" -WingetId "Git.Git"
        Write-Host "Cloning vcpkg into '$($env:VCPKG_ROOT)'..."
        & git clone https://github.com/microsoft/vcpkg $env:VCPKG_ROOT
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to clone vcpkg repository."
        }
    }

    $vcpkgExe = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        $bootstrapScript = Join-Path $env:VCPKG_ROOT "bootstrap-vcpkg.bat"
        if (-not (Test-Path $bootstrapScript)) {
            throw "bootstrap-vcpkg.bat not found in '$($env:VCPKG_ROOT)'."
        }

        Write-Host "Bootstrapping vcpkg..."
        & $bootstrapScript -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg bootstrap failed."
        }
    }
}

function Unblock-BuildArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$BuildDir
    )

    if ($SkipUnblock) {
        Write-Host "Skipping artifact unblock step."
        return
    }

    if (-not (Test-Path $BuildDir)) {
        return
    }

    $artifacts = Get-ChildItem -Path $BuildDir -Recurse -File -Include *.exe,*.dll -ErrorAction SilentlyContinue
    if (-not $artifacts) {
        return
    }

    foreach ($artifact in $artifacts) {
        try {
            Unblock-File -Path $artifact.FullName -ErrorAction Stop
        }
        catch {
            Write-Warning "Could not unblock '$($artifact.FullName)': $($_.Exception.Message)"
        }
    }

    Write-Host "Unblock step completed for $($artifacts.Count) artifact(s)."
}

function Resolve-SignToolPath {
    $cmd = Get-Command signtool -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $sdkRoots = @(
        "C:\Program Files (x86)\Windows Kits\10\bin",
        "C:\Program Files\Windows Kits\10\bin"
    )

    foreach ($root in $sdkRoots) {
        if (Test-Path $root) {
            $candidate = Get-ChildItem -Path $root -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -like "*\\x64\\signtool.exe" } |
                Select-Object -First 1
            if ($candidate) {
                return $candidate.FullName
            }
        }
    }

    return $null
}

function Get-CodeSigningCertificate {
    param(
        [Parameter(Mandatory = $true)][string]$Thumbprint
    )

    $cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
        Where-Object { $_.Thumbprint -eq $Thumbprint } |
        Select-Object -First 1
    if ($cert) {
        return $cert
    }

    $cert = Get-ChildItem Cert:\LocalMachine\My -CodeSigningCert -ErrorAction SilentlyContinue |
        Where-Object { $_.Thumbprint -eq $Thumbprint } |
        Select-Object -First 1
    return $cert
}

function Invoke-SignFile {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string]$Thumbprint,
        [Parameter(Mandatory = $true)]$Certificate,
        [string]$SignToolPath
    )

    if ($SignToolPath) {
        & "$SignToolPath" sign /sha1 $Thumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 "$FilePath" | Out-Null
        return ($LASTEXITCODE -eq 0)
    }

    try {
        $sigResult = Set-AuthenticodeSignature -FilePath $FilePath -Certificate $Certificate -HashAlgorithm SHA256 -TimestampServer $TimestampUrl
        return ($sigResult.Status -eq "Valid")
    }
    catch {
        return $false
    }
}

function Try-CodeSignArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$BuildDir
    )

    $thumb = $CodeSignThumbprint

    if ($UseDebugCodeSign -and [string]::IsNullOrWhiteSpace($thumb)) {
        $thumb = Ensure-DebugCodeSigningCertificate
    }

    if ([string]::IsNullOrWhiteSpace($thumb)) {
        $thumb = $env:CODE_SIGN_CERT_THUMBPRINT
    }

    if ([string]::IsNullOrWhiteSpace($thumb)) {
        Write-Host "Code-sign step skipped (no certificate thumbprint provided)."
        return
    }

    $cert = Get-CodeSigningCertificate -Thumbprint $thumb
    if (-not $cert) {
        throw "Code-sign certificate with thumbprint '$thumb' was not found in CurrentUser\\My or LocalMachine\\My."
    }

    $signTool = Resolve-SignToolPath

    $artifacts = Get-ChildItem -Path $BuildDir -Recurse -File -Include *.exe,*.dll -ErrorAction SilentlyContinue
    if (-not $artifacts) {
        return
    }

    if (-not $signTool) {
        Write-Warning "signtool.exe not found; using Set-AuthenticodeSignature fallback."
    }

    foreach ($artifact in $artifacts) {
        $ok = Invoke-SignFile -FilePath $artifact.FullName -Thumbprint $thumb -Certificate $cert -SignToolPath $signTool
        if (-not $ok) {
            Write-Warning "Signing failed for '$($artifact.FullName)'."
        }
    }

    Write-Host "Code-sign step attempted for $($artifacts.Count) artifact(s)."
}

    function Ensure-DebugCodeSigningCertificate {
        $subject = "CN=$DebugCertSubject"

        $existing = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
            Where-Object { $_.Subject -eq $subject } |
            Sort-Object NotAfter -Descending |
            Select-Object -First 1

        if (-not $existing) {
            Write-Host "Creating debug code-signing certificate '$subject'..."
            $existing = New-SelfSignedCertificate `
                -Type CodeSigningCert `
                -Subject $subject `
                -CertStoreLocation "Cert:\CurrentUser\My" `
                -NotAfter (Get-Date).AddYears(3) `
                -KeyAlgorithm RSA `
                -KeyLength 3072 `
                -HashAlgorithm SHA256
        }

        # Make the self-signed cert trusted for this user profile.
        if (-not (Get-ChildItem Cert:\CurrentUser\Root -ErrorAction SilentlyContinue | Where-Object { $_.Thumbprint -eq $existing.Thumbprint })) {
            $null = Export-Certificate -Cert $existing -FilePath "$env:TEMP\bt-debug-signing.cer" -Force
            $null = Import-Certificate -FilePath "$env:TEMP\bt-debug-signing.cer" -CertStoreLocation "Cert:\CurrentUser\Root"
        }

        if (-not (Get-ChildItem Cert:\CurrentUser\TrustedPublisher -ErrorAction SilentlyContinue | Where-Object { $_.Thumbprint -eq $existing.Thumbprint })) {
            $null = Export-Certificate -Cert $existing -FilePath "$env:TEMP\bt-debug-signing.cer" -Force
            $null = Import-Certificate -FilePath "$env:TEMP\bt-debug-signing.cer" -CertStoreLocation "Cert:\CurrentUser\TrustedPublisher"
        }

        return $existing.Thumbprint
    }

function Resolve-BuiltExePath {
    $ninjaPath = Join-Path (Resolve-Path .\build).Path "bt\bt.exe"
    if (Test-Path $ninjaPath) {
        return $ninjaPath
    }

    $multiConfigPath = Join-Path (Resolve-Path .\build).Path "bt\$Config\bt.exe"
    if (Test-Path $multiConfigPath) {
        return $multiConfigPath
    }

    return $null
}

function Show-SignatureStatus {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath
    )

    $sig = Get-AuthenticodeSignature -FilePath $ExePath
    Write-Host "Signature status for '$ExePath': $($sig.Status)"

    if ($sig.Status -ne "Valid") {
        Write-Warning "Smart App Control may block unsigned/untrusted binaries. For production, sign bt.exe with an OV/EV code-signing certificate and timestamp it."
    }
}

function Create-ProfilePickerShortcut {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath
    )

    if ($SkipShortcut) {
        Write-Host "Skipping shortcut creation step."
        return
    }

    $targetShortcut = $ShortcutPath
    if ([string]::IsNullOrWhiteSpace($targetShortcut)) {
        $desktop = [Environment]::GetFolderPath("Desktop")
        $targetShortcut = Join-Path $desktop "Browser Profile Picker.lnk"
    }

    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($targetShortcut)
    $shortcut.TargetPath = $ExePath
    $shortcut.Arguments = "pick `"$PickerUrl`""
    $shortcut.WorkingDirectory = Split-Path -Parent $ExePath
    $shortcut.IconLocation = "$ExePath,0"
    $shortcut.Description = "Open Browser Tamer profile picker and then open selected profile with URL"
    $shortcut.Save()

    Write-Host "Created profile picker shortcut: $targetShortcut"
}

function New-ProductionPackage {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath
    )

    if (-not $CreateProductionPackage) {
        return
    }

    $thumb = $CodeSignThumbprint
    if ([string]::IsNullOrWhiteSpace($thumb)) {
        $thumb = $env:CODE_SIGN_CERT_THUMBPRINT
    }
    if ([string]::IsNullOrWhiteSpace($thumb)) {
        throw "CreateProductionPackage requires a real code-signing certificate thumbprint via -CodeSignThumbprint or CODE_SIGN_CERT_THUMBPRINT."
    }

    $cert = Get-CodeSigningCertificate -Thumbprint $thumb
    if (-not $cert) {
        throw "Production signing cert '$thumb' not found in CurrentUser\\My or LocalMachine\\My."
    }

    if ($cert.Subject -eq $cert.Issuer) {
        throw "Production package cannot use self-signed certificates. Use an OV/EV certificate trusted by Smart App Control."
    }

    $signTool = Resolve-SignToolPath
    if (-not $signTool) {
        Write-Warning "signtool.exe not found; production signing will use Set-AuthenticodeSignature fallback."
    }

    $signed = Invoke-SignFile -FilePath $ExePath -Thumbprint $thumb -Certificate $cert -SignToolPath $signTool
    if (-not $signed) {
        throw "Production signing failed for '$ExePath'."
    }

    $sig = Get-AuthenticodeSignature -FilePath $ExePath
    if ($sig.Status -ne "Valid") {
        throw "Signed executable verification failed with status '$($sig.Status)'."
    }

    $outputRoot = $ProductionOutputDir
    if (-not [System.IO.Path]::IsPathRooted($outputRoot)) {
        $outputRoot = Join-Path (Get-Location).Path $outputRoot
    }

    $version = $env:VERSION
    if ([string]::IsNullOrWhiteSpace($version)) {
        $version = "dev"
    }

    $pkgDir = Join-Path $outputRoot $version
    New-Item -ItemType Directory -Path $pkgDir -Force | Out-Null

    $exeName = Split-Path -Leaf $ExePath
    $pkgExePath = Join-Path $pkgDir $exeName
    Copy-Item -Path $ExePath -Destination $pkgExePath -Force

    $zipPath = Join-Path $outputRoot "bt-$version-windows-x64.zip"
    if (Test-Path $zipPath) {
        Remove-Item $zipPath -Force
    }
    Compress-Archive -Path $pkgExePath -DestinationPath $zipPath -CompressionLevel Optimal

    $hash = Get-FileHash -Path $zipPath -Algorithm SHA256
    $hashLine = "$($hash.Hash)  $([System.IO.Path]::GetFileName($zipPath))"
    $hashPath = "$zipPath.sha256"
    Set-Content -Path $hashPath -Value $hashLine -NoNewline

    Write-Host "Production package created: $zipPath"
    Write-Host "SHA256 file created: $hashPath"
}

Ensure-CommandInstalled -Command "cmake" -WingetId "Kitware.CMake"

if ($Generator -eq "Ninja") {
    Ensure-CommandInstalled -Command "ninja" -WingetId "Ninja-build.Ninja"
}

if ($IsWindows) {
    Ensure-BuildTools
}

Ensure-Vcpkg

$toolchain = Join-Path $env:VCPKG_ROOT "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $toolchain)) {
    throw "Vcpkg toolchain file not found at '$toolchain'. Verify VCPKG_ROOT."
}

$mdVersion = (Get-Content -Path .\docs\release-notes.md -First 1).Trim('#', ' ')
Write-Host "Building version $mdVersion"
$env:VERSION = $mdVersion

if (Test-Path .\pre-build.ps1) {
    .\pre-build.ps1
}

if ($IsWindows -and $Generator -eq "Ninja") {
    $vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        throw "VsDevCmd not found at '$vsDevCmd'. Install Visual Studio Build Tools C++ workload."
    }

    $configureCmd = "cmake -S . -B build -G Ninja -D CMAKE_BUILD_TYPE=$Config -D CMAKE_TOOLCHAIN_FILE=$toolchain -D VCPKG_TARGET_TRIPLET=$Triplet"
    $buildCmd = "cmake --build build --config $Config --target $Target"
    cmd.exe /c "call `"$vsDevCmd`" -arch=$Architecture -host_arch=$Architecture && $configureCmd && $buildCmd"
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
}
else {
    cmake -B build -S . -G "$Generator" -A "$Architecture" `
        -D "CMAKE_BUILD_TYPE=$Config" `
        -D "CMAKE_TOOLCHAIN_FILE=$toolchain" `
        -D "VCPKG_TARGET_TRIPLET=$Triplet"

    cmake --build build --config $Config --target $Target
}

Unblock-BuildArtifacts -BuildDir (Resolve-Path .\build).Path
Try-CodeSignArtifacts -BuildDir (Resolve-Path .\build).Path

$builtExe = Resolve-BuiltExePath
if (-not $builtExe) {
    throw "Could not find built bt.exe in build output."
}

Show-SignatureStatus -ExePath $builtExe
Create-ProfilePickerShortcut -ExePath $builtExe
New-ProductionPackage -ExePath $builtExe

if ($RunTests) {
    if ($IsWindows -and $Generator -eq "Ninja") {
        $vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
        cmd.exe /c "call `"$vsDevCmd`" -arch=$Architecture -host_arch=$Architecture && cmake --build build --config $Config --target test"
        if ($LASTEXITCODE -ne 0) {
            throw "Building test target failed with exit code $LASTEXITCODE"
        }
    }

    $testExe = Join-Path (Resolve-Path .\build).Path "test\$Config\test.exe"
    if (Test-Path $testExe) {
        & $testExe
    }
    else {
        Write-Warning "Tests executable not found at $testExe"
    }
}