---
title: 在 WSL 下进行 FPGA 开发
date: 2023-12-01 12:00:00 +0800
categories:
  - 环境配置
  - 虚拟机
tags:
  - FPGA
toc: true
description: 本指南介绍如何在 WSL 下的 Linux 发行版进行 FPGA 开发
---

本指南介绍如何在 WSL 下的 Linux 发行版进行 FPGA 开发

## 安装软件

**Vivado**

[安装 Vivado 2019.2 WebPACK](https://gitee.com/foxtrot024/RVfpga_SoC/tree/lab0#1--vivado-%E5%AE%89%E8%A3%85)

**VS Code**

[安装 VS Code](https://code.visualstudio.com/docs/setup/linux)

> 虽然 Microsoft 推荐在 Windows 上连接 WSL 进行远程开发，但这里仍然强烈建议直接在 WSL 使用 Linux 版本的 VS Code 。
{: .prompt-warning }

**PlatfromIO IDE**

1. 执行：
	```bash
	sudo apt-get install python3-venv
	```

2. 在 VS Code 安装 [PlatfromIO IDE](https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation) 扩展。

**OpenOCD**

[安装 OpenOCD](https://gitee.com/foxtrot024/RVfpga_SoC/tree/lab0#4--%E5%AE%89%E8%A3%85openocd)

## 连接 USB 设备

按以下步骤安装 USBIPD-WIN 和 USBID

1. **在 Windows 中**

   安装 [USBIPD-WIN](https://github.com/dorssel/usbipd-win/releases)

2. **在 Linux 中**

   执行：

   ```bash
   sudo apt install linux-tools-generic hwdata
   sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*-generic/usbip 20
   ```

3. **在 Windows 中**

   1. 管理员身份打开 PowerShell ，执行：
		```shell
		usbipd wsl list
		```

   2. 选择要附加到 WSL 的设备总线 ID，然后执行：
		```shell
		usbipd wsl attach --busid <busid> --distribution Ubuntu
		```

4. **在 Linux 中**

   1. 列出连接的 USB 设备：
		```bash
		lsusb
		```

   2. 物理断开设备，或者使用以下指令：
		```
		usbipd wsl detach --busid <busid>
		```

## 使用 PlatfromIO

1. 列出连接的 USB 设备：
	```bash
	lsusb
	```
	
2. 修改设备的访问权限：
	```bash
	sudo usermod -aG plugdev $USER
	```
	或
	```bash
	sudo chmod -R 777 /dev/bus/usb/
	```