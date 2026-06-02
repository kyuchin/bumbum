Write-Host "========================================"
Write-Host " ServiceHost - Building..."
Write-Host "========================================"

$clPath = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64\cl.exe"
$linkPath = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64\link.exe"

if (-not (Test-Path $clPath)) {
    Write-Host "[ERROR] MSVC compiler not found at $clPath"
    exit 1
}

Write-Host "[OK] MSVC compiler found"

$sources = @(
    "main.cpp",
    "MainWindow.cpp",
    "TrayManager.cpp",
    "ScheduleManager.cpp",
    "ConfigManager.cpp",
    "Logger.cpp",
    "ProcessHelper.cpp",
    "Watchdog.cpp"
)

$cwd = "C:\Yaptigim\acma"
$srcFiles = $sources | ForEach-Object { "$cwd\$_" }
$outExe = "$cwd\ServiceHost.exe"

$libs = "user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib shlwapi.lib advapi32.lib"

Write-Host "Compiling..."
$objFiles = @()
$compileOk = $true
foreach ($src in $sources) {
    $obj = "$cwd\$($src -replace '\.cpp$', '.obj')"
    $objFiles += $obj
    # delete old obj so we can detect if compilation fails
    if (Test-Path $obj) { Remove-Item $obj -Force }
}

# Compile all at once with cl
$clArgs = @("/nologo", "/O2", "/EHsc", "/W3", "/utf-8")
$clArgs += $srcFiles
$clArgs += @("/link", "/SUBSYSTEM:WINDOWS")
$clArgs += $libs.Split(' ')
$clArgs += "/OUT:$outExe"

# Need to run in cmd environment for proper arg handling
$cmdLine = "`"$clPath`" /nologo /O2 /EHsc /W3 /utf-8 $($srcFiles -join ' ') /link /SUBSYSTEM:WINDOWS $libs /OUT:`"$outExe`""

Write-Host "Running: cl.exe /nologo /O2 /EHsc /W3 /utf-8 ..."

$process = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", $cmdLine -NoNewWindow -Wait -PassThru

if ($process.ExitCode -eq 0) {
    Write-Host ""
    Write-Host "========================================"
    Write-Host " BUILD SUCCESSFUL - ServiceHost.exe"
    Write-Host "========================================"
} else {
    Write-Host ""
    Write-Host "========================================"
    Write-Host " BUILD FAILED! Error code: $($process.ExitCode)"
    Write-Host "========================================"
    exit $process.ExitCode
}
