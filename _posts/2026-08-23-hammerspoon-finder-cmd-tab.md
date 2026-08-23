---
title: 用 Hammerspoon 把 Finder 的 Cmd+数字 从切视图改成切标签页
date: 2026-08-23 01:15:00 +0800
categories:
  - 环境配置
  - macOS
tags:
  - Hammerspoon
description: 用 Hammerspoon eventtap 脚本让 Finder 的 Cmd+1/2/3/4 从切换视图模式变成切换标签页。
---

## 问题

macOS Finder 里 `Cmd+1~4` 是切换视图（图标 / 列表 / 分栏 / 画廊），而其他大部分应用里 `Cmd+1~9` 是切换标签页。尽管从 macOS 10.12 起，Finder 终于有了标签页，但直到 26.5.2 都没有对应的 `Cmd+数字` 快捷键——切标签页只能 `Ctrl+Tab` 逐个轮换，或 `Cmd+Shift+\` 打开标签概览再选。

## 方案选型

能拦全局键盘事件的工具，macOS 上主流有三个：

| 工具 | 机制 | 结论 |
|---|---|---|
| Karabiner-Elements | 内核级驱动层改键 | 能做但重：改的是全系统键位，按前台应用区分行为要写复杂条件，且常驻一个虚拟驱动 |
| BetterTouchTool | GUI 配置 | 闭源付费，动作模型偏 GUI 自动化，版本化/分享配置不如纯文本 |
| Hammerspoon | `hs.eventtap`（CGEventTap）+ Lua | 纯文本配置、可版本化、能按 bundleID 区分前台应用 |

选 Hammerspoon：

1. **`hs.eventtap` 拦截 keyDown**——注意不是 `hs.hotkey`。`hs.hotkey` 是热键注册，系统会把 `Cmd+数字` 原样传给 Finder；eventtap 在事件进入目标应用前拿到它，返回 `true` 可以吞掉原事件，Finder 就不会执行原生的切视图行为。
2. **Accessibility API 找到标签页并点击**——Finder 的标签栏在 AX 树里是一个 `AXTabGroup`，其 `AXTabs` 属性是各标签（`AXRadioButton`）的列表。递归遍历窗口的 AX 树找到它，对第 n 个 tab 调 `doAXPress()` 即等效于点了一下那个标签。

判定条件：仅当前台应用是 `com.apple.finder`、修饰键**只有** Cmd、键码属于数字 1~9 时才拦截并吞掉原事件；其余情况一律放行。这样不影响其他应用的 `Cmd+数字`，也不干扰 Finder 里 `Cmd+Shift+数字` 之类的组合。

## 代码

<script src="https://gist.github.com/Yiipu/9508a08b663d8ca6e23bf9d639b74788.js"></script>

两个细节：

**为什么整个回调包在 `pcall` 里。** eventtap 的回调一旦抛错，macOS 会静默禁用整个 tap——没有通知、没有日志，表现为快捷键"突然死了"。`pcall` 把错误拦在 Lua 层，回调永远正常返回，tap 就不会被系统拆掉。这是 Hammerspoon 社区里血泪换来的惯例。

**为什么还挂一个 watchdog。** 即使回调不抛错，tap 也可能被系统禁用（比如 Secure Input 被某应用开启、系统注入错误事件）。脚本里每 5 秒检查一次 `finderTap:isEnabled()`，死了就 `start()` 复活：

```lua
hs.timer.doEvery(5, function()
    if finderTap and not finderTap:isEnabled() then
        finderTap:start()
    end
end)
```

## 已知问题：不明原因的间歇性失效

这个脚本出现过两次失效：现象是大约 30 次 `Cmd+数字` 切换后，快捷键不再响应，但重载 Hammerspoon 配置后恢复。重复"重载 → 按 30 次"两轮，都在相近次数处失效。

随后我加了逐键日志（回调是否触发 / AXTabGroup 是否找到 / 报错路径）排查，**但没有复现**——插桩后连续 100+ 次切换零失败。提两种假设，均未证实：

1. 失效是非确定性的，"约 30 次"是巧合，真实触发条件是某种系统状态（回调超时被禁 tap、Secure Input、其他程序抢占事件 tap），插桩改变了时序后不再触发；
2. 环境因素（如某次 Hammerspoon 重启）已经自行消除了病因。

watchdog + `pcall` 的组合理论上能覆盖"tap 被系统禁用"这条路径，但如果失效点其实在 AX 查找层（tap 活着但找不到 tab group），watchdog 是救不了的——这是当前方案的已知盲区。如果读者遇到同样问题，欢迎在 gist 评论里提供线索。

## 使用前提

- Hammerspoon 需要辅助功能权限（系统设置 → 隐私与安全性 → 辅助功能），首次运行会弹窗引导；
- 脚本只处理单窗口的 `focusedWindow`。Finder 多窗口时，作用于当前聚焦窗口，与原生行为一致；
- 依赖 Finder 标签栏存在于 AX 树中的 `AXTabGroup`——这是当前版本的实现细节，macOS 大版本更新后若失效，优先检查这里。

## 小结

一个不到 60 行的脚本，把 macOS 一个存在了近十年、且官方不给改的键位冲突，在事件层解决了。比方案本身更想分享的是两条经验：

- **eventtap 回调必须 `pcall`**，否则一次 Lua 错误就静默报废整个 tap；
- **调试插桩本身会改变被调试系统**——我第一次把日志写进 `~/.hammerspoon/`，触发自动重载的 pathwatcher，配置每秒重载十次，制造了一个全新的 bug。观测工具影响被观测对象，在 GUI 事件层同样成立。
