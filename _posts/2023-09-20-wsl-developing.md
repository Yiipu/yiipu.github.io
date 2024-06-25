---
title: 使用 WSL 进行编程
author: yiipu
date: 2023-09-20 12:00:00 +0800
last_modified_at: 2023-09-20 12:00:00 +0800
categories: [环境配置, 虚拟机, WSL]
description: 在 WSL 编程的一些tips
---

> 安装 WSL 的教程可以参考 [Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/wsl/install)
{: .prompt-tip }

## 显示汉字

使用 symbolic link 将 Windows 系统的字体目录链接到 Ubuntu 中的字体目录，以便在 WSL 中使用 Windows 字体。

```bash
sudo ln -s /mnt/c/Windows/Fonts /usr/share/fonts/font
fc-cache -fv
```

## 如果 `github.com` 解析到 `127.0.0.1`

> 在 WSL 中使用 Git 的教程可以参考 [Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/wsl/tutorials/wsl-git) 。
{: .prompt-tip }

如果你恰好也在使用 [`Watt Toolkit (原名 Steam++)`](https://github.com/BeyondDimension/SteamTools) ， 那么执行 `ping github.com` 命令，可能会发现 `github.com` 被解析为 `127.0.0.1` 。

这是因为 `Watt Toolkit` 修改了主机的 Host 文件，而后者被 WSL 继承。

执行以下操作停用继承：

1. 用文本编辑器（如 `vim`）打开 `/etc/wsl.conf`

2. 在 `[network]` 下添加新行 `generateHosts = false`

3. 用文本编辑器打开 `/etc/hosts` 并删除由 `Watt Toolkit` 生成的部分。

