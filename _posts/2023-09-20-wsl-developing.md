---
title: 使用 WSL 进行编程
date: 2023-09-20 12:00:00 +0800
last_modified_at: 2023-09-20 12:00:00 +0800
categories:
  - 环境配置
  - WSL
tags:
  - WSL
  - Windows
description: 在 WSL 编程的一些实用技巧
---

> **提示**: 安装 WSL 的详细教程可参考 [Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/wsl/install)
{: .prompt-tip }

## 显示汉字

为了让 WSL 中的应用程序能够正确显示中文字体，可以通过符号链接将 Windows 系统的字体目录链接到 Ubuntu 中的字体目录：

```bash
# 创建符号链接以访问 Windows 字体
sudo ln -s /mnt/c/Windows/Fonts /usr/share/fonts/windows-fonts

# 更新字体缓存
sudo fc-cache -fv
```

执行上述命令后，重启终端即可使中文字体生效。

## 解决 `github.com` 解析到 `127.0.0.1` 的问题

> **提示**: 在 WSL 中使用 Git 的最佳实践可参考 [Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/wsl/tutorials/wsl-git)
{: .prompt-tip }

如果你正在使用 [`Watt Toolkit (原名 Steam++)`](https://github.com/BeyondDimension/SteamTools)，可能会遇到 `github.com` 被解析为 `127.0.0.1` 的问题。

这通常是由于 `Watt Toolkit` 修改了主机的 Hosts 文件，而该文件被 WSL 自动继承所致。

### 解决方案

要解决此问题，请按以下步骤停用 WSL 对 Hosts 文件的自动继承：

1. **编辑 WSL 配置文件**

   ```bash
   sudo vim /etc/wsl.conf
   ```

2. **禁用 Hosts 文件自动生成**

   在 `[network]` 部分下添加以下行：
   
   ```ini
   [network]
   generateHosts = false
   ```

3. **清理现有错误条目**

   手动编辑 `/etc/hosts` 文件，删除由 `Watt Toolkit` 添加的相关条目：
   
   ```bash
   sudo vim /etc/hosts
   ```

4. **重启 WSL 实例**

   ```bash
   # 在 PowerShell 中执行
   wsl --shutdown
   # 然后重新启动 WSL
   ```

完成上述步骤后，`github.com` 应能正常解析，Git 相关操作也能恢复正常。