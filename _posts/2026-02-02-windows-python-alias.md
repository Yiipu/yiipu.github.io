---
layout: post
title: "Windows 终端运行 python 打开 MS Store 的解决方法"
date: 2026-02-02
categories: [环境配置, Windows]
tags: [Windows, Python, 环境变量]
---

## 现象 

Windows 下手动安装 Python 后命令行运行 `python` 或 `python3` 命令会打开 MS Store。 

## 原因

- MS Store 存在应用执行别名 `python.exe` 等。
- 这些别名指向的程序是一种占位符，它们:
  - 打开 MS Store 中相应程序的页面(如 `python`)。
  - 存在于 `WindowsApps`(`%USERPROFILE%\AppData\Local\Microsoft\WindowsApps`)。
  - 受控于应用执行别名功能。
- **关键**：环境变量 `PATH` 中 `WindowsApps` 比手动添加的项目更靠前（优先级更高）。

参考资料: 

- [MS Learn \| Why does running python.exe open the Microsoft Store?](https://learn.microsoft.com/en-us/windows/python/faqs#why-does-running-python-exe-open-the-microsoft-store-)

## 解决方案

### 方案 1：禁用应用执行别名

1. 打开设置，前往`应用-高级应用设置-应用执行别名`。
2. 找到`应用安装程序`（即 MS Store），关闭其别名`python.exe`、`python3.exe`。

### 方案 2：调整环境变量 `PATH` 顺序。

> 脚本有风险，使用须谨慎。
{: .prompt-warning }

```shell
# 调整 PATH 环境变量，将 Python\bin 放在 WindowsApps 之前
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathArray = $userPath -split ';'

# 找到 Python\bin 和 WindowsApps 路径
$pythonPath = $pathArray | Where-Object { $_ -like '*Python\bin' }
$windowsAppsPath = $pathArray | Where-Object { $_ -like '*WindowsApps' }

# 移除这两个路径，准备重新排序
$newPathArray = $pathArray | Where-Object { 
    $_ -ne $pythonPath -and $_ -ne $windowsAppsPath 
}

# 将 Python\bin 放在最前面，WindowsApps 其次，然后是其他路径
$newPath = ($pythonPath, $windowsAppsPath) + $newPathArray
$newPathString = $newPath -join ';'

# 更新环境变量
[Environment]::SetEnvironmentVariable("Path", $newPathString, "User")

Write-Host "PATH 已更新，Python\bin 现在在 WindowsApps 之前"
```
