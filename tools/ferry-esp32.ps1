param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('build', 'flash')]
    [string]$Action
)

$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot '..\poc\esp32-classic-hid-host'
$python = 'E:\ESP-IDF\.espressif\python_env\idf6.1_py3.11_env\Scripts\python.exe'
$idf = 'E:\ESP-IDF\esp\v6.1\esp-idf\tools\idf.py'

$env:IDF_PATH = 'E:\ESP-IDF\esp\v6.1\esp-idf'
$env:IDF_TOOLS_PATH = 'E:\ESP-IDF\.espressif'
$env:IDF_PYTHON_ENV_PATH = 'E:\ESP-IDF\.espressif\python_env\idf6.1_py3.11_env'
$env:ESP_IDF_VERSION = '6.1'
$env:ESP_ROM_ELF_DIR = 'E:\ESP-IDF\.espressif\tools\esp-rom-elfs\20241011'
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYS -ErrorAction SilentlyContinue
Remove-Item Env:MINGW_PREFIX -ErrorAction SilentlyContinue
$env:Path = 'E:\ESP-IDF\.espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;' + $env:Path

& $python $idf -C $project build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Action -eq 'build') { exit 0 }

$deadline = (Get-Date).AddMinutes(1)
$attempt = 0
while ((Get-Date) -lt $deadline) {
    $attempt++
    Write-Output "FLASH_ATTEMPT_$attempt"
    & $python $idf -C $project -p COM9 flash
    if ($LASTEXITCODE -eq 0) {
        Write-Output 'FLASH_SUCCEEDED'
        exit 0
    }
    Start-Sleep -Seconds 2
}

Write-Error 'FLASH_TIMEOUT: COM9 remained busy for one minute'
exit 1
