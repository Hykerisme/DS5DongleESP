# ESP32 Classic Bluetooth HID Host POC-2

This project targets the original ESP32-D0WD and uses ESP-IDF v6.1 Bluedroid/`esp_hidh` to discover and open Classic Bluetooth HID devices. It prints raw HID input and feature reports on the CH9102 console.

UART2 is reserved for the inter-chip link:

```text
TX GPIO17, RX GPIO16, RTS GPIO15, CTS GPIO4, READY GPIO2
```

The UART link is initialized separately from the console. The Bluetooth raw-report path is the first validation target; DualSense-specific output translation and full UART forwarding follow after pairing and input reports are confirmed.

Build and flash on COM9 from an ESP-IDF v6.1 PowerShell:

```powershell
$env:MSYSTEM = $null
$env:MSYS = $null
$env:IDF_TOOLS_PATH = "E:\ESP-IDF\.espressif"
$env:IDF_PATH = "E:\ESP-IDF\esp\v6.1\esp-idf"
. "$env:IDF_PATH\export.ps1"
cd E:\Code\git_projects\esp\DS5DongleESP\poc\esp32-classic-hid-host
idf.py set-target esp32
idf.py build
idf.py -p COM9 flash monitor
```
