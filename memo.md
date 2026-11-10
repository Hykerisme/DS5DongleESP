# 项目环境备忘录

- ESP-IDF 不是标准用户目录安装。实际 IDF 源码位于 `E:\ESP-IDF\esp\v6.1\esp-idf`，工具、Python 环境和缓存位于 `E:\ESP-IDF\.espressif`。
- 使用 ESP-IDF 前应显式设置 `IDF_TOOLS_PATH=E:\ESP-IDF\.espressif`，并通过 PowerShell (`pwsh`) 加载 `export.ps1`；不要假设工具位于 `C:\Users\liuji\.espressif`。
- Python 依赖使用 `uv` 管理；不要使用裸系统 Python，也不要未经确认重新安装或下载 ESP-IDF 工具。
- Node.js 依赖使用 Volta 管理；执行 debugger 的 Node/npm 命令前使用 Volta 环境，不要依赖系统 Node。
- 原始 `dualsense-tester` 与独立 `dongle-debugger` 是两个项目。后续 debugger 修改只能发生在本仓库的 `dongle-debugger` 目录，不要修改原 tester。
