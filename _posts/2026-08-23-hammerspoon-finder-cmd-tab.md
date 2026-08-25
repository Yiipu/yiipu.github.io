---
title: 用 Hammerspoon 把 Finder 的 Cmd+数字 从切视图改成切标签页
date: 2026-08-23 01:15:00 +0800
last_modified_at: 2026-08-25 22:30:00 :+0800
categories:
  - 环境配置
  - macOS
tags:
  - Hammerspoon
description: 用 Hammerspoon eventtap 让 Finder 的 Cmd+1~9 切标签页，右 Cmd+数字保留原生切视图。含权限坑与 tap 静默失效的根因复盘。
---

## 问题

macOS Finder 里 `Cmd+1~4` 是切换视图（图标 / 列表 / 分栏 / 画廊），而其他大部分应用里 `Cmd+1~9` 是切换标签页。Finder 从 OS X 10.9 Mavericks（2013 年）起就有了标签页，但直到本文写作时的 macOS 26.5 都没有对应的 `Cmd+数字` 快捷键——切标签页只能 `Ctrl+Tab` 逐个轮换，或 `Cmd+Shift+\` 打开标签概览再选。

## 方案选型

能拦全局键盘事件的工具，macOS 上主流有三个：

| 工具 | 机制 | 结论 |
|---|---|---|
| Karabiner-Elements | 虚拟键盘驱动层改键 | 能做但绕：规则是嵌套 JSON，按前台应用分流要写 [`frontmost_application_if`](https://karabiner-elements.pqrs.org/docs/json/complex-modifications-manipulator-definition/conditions/frontmost-application/) 条件；动作模型只会"发键 / 发事件"，点不到 AX 标签，要切第 n 个标签还得外挂脚本；且常驻一个虚拟驱动 |
| BetterTouchTool | GUI 配置 | 闭源付费（标准授权 $10 上下，45 天试用），配置可导出但版本化 / diff / 分享不如纯文本顺手 |
| Hammerspoon | `hs.eventtap`（CGEventTap）+ Lua | 纯文本配置、可版本化、能按前台应用 bundleID 区分，还能直接调 Accessibility API 点标签 |

选了 Hammerspoon：拦键和点 UI 两件事一个工具全覆盖，整个配置就是一个 Lua 文件。

## 功能描述

装上脚本后，Finder 里的行为变成：

- **左 Cmd+1~9**：切到第 n 个标签页（标签不足 n 个时无动作）；
- **右 Cmd+1~9**：保持原生行为——切视图（图标 / 列表 / 分栏 / 画廊）；
- **其他场景不受影响**：前台不是 Finder、或带了 `Shift` / `Option` / `Ctrl` 等其他修饰键、或纯数字输入，一律原样放行；
- 多窗口时作用于当前聚焦的窗口，与原生行为一致。

前提是给 Hammerspoon 授权**辅助功能**和**输入监控**两项权限（原因见调试历程）。

## 实现

### 为什么是 eventtap 而不是 hs.hotkey

`hs.hotkey` 底层是 Carbon 的 `RegisterEventHotKey`：热键命中时会被系统截获、不会传给前台应用——所以它其实"拦得住"。真正的问题有三：修饰键按标志匹配，**分不出左右 Cmd**；注册即全局生效，要"仅 Finder 前台时介入"就得自己维护热键的开关状态；也没法在"吞掉 / 放行 / 改写"之间逐事件决策。

`hs.eventtap` 则直接包装 CGEventTap（[源码](https://github.com/Hammerspoon/hammerspoon/blob/master/extensions/eventtap/libeventtap.m)里就是 `CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, ...)`）：事件在送达目标应用之前先经过回调，返回 `true` 吞掉，返回 `false` 放行。

找到并点击标签页用的是 Accessibility API：Finder 的标签栏在 AX 树里是一个 `AXTabGroup`，其 `AXTabs` 属性是各标签（`AXRadioButton`）的列表。递归遍历窗口的 AX 树找到它，对第 n 个 tab 调 `doAXPress()` 即等效于点了一下那个标签。

左右 Cmd 的区分：回调同时监听 `keyDown` 和 `flagsChanged`，自己维护 keycode 54（右 Cmd）的按下 / 抬起状态；`flagsChanged` 一律放行，只用来记账。

### 源码

<script src="https://gist.github.com/Yiipu/9508a08b663d8ca6e23bf9d639b74788.js"></script>

### 几个关键细节

- **整个回调包在 `pcall` 里。** 这不是 Hammerspoon 的硬性要求——读它的源码会发现，eventtap 回调在 C 层本来就被 `protectedCallAndTraceback` 保护，Lua 抛错只会往控制台打一行日志，tap 并不会死（在 1.1.1 上实测：回调连续抛错后，tap 照常收事件）。包 `pcall` 的实际收益有二：不包的话，错误路径会把触发那次按键吞掉（C 层错误分支返回 NULL，即删除当前事件），包了可以自己决定放行；另外错误日志可以换成自己的格式。
- **watchdog 是冗余保险。** tap 确实可能被系统禁用（回调太慢等场景），但 Hammerspoon 收到禁用通知（`kCGEventTapDisabledByTimeout` / `kCGEventTapDisabledByUserInput`）时会自动重启 tap——源码注释原话："apparently OS X disables eventtaps if it thinks they are slow or odd or just because the moon is wrong in some way... but at least it's nice enough to tell us"。每 5 秒的 `isEnabled()` 自检等于又上了一道保险，代价可以忽略。
- **右 Cmd 分支重发的事件必须定向 `post(finderApp)`。** 实测（用一个观测 tap）：全局 `post()` 的合成事件会重新流回 tap 链——对这个脚本来说就是被自己的回调再截一次，形成自触发循环；而 `post(app)` 走 `CGEventPostToPid`，完全绕过 tap 链直达 Finder，不会被自己再吃掉。

### 已知边界

- 依赖 Finder 标签栏存在于 AX 树中的 `AXTabGroup`——这是当前版本的实现细节，macOS 大版本更新后若失效，优先检查这里；
- 缺输入监控权限时 tap 不收事件，但 `isEnabled()` 仍返回 true——快捷键静默失效时先查这里；
- 脚本只处理单窗口的 `focusedWindow`。

## 调试历程

### 间歇性失效

这个脚本曾出现间歇性静默失效：快捷键不再响应，但重载配置后恢复。最终定位是 Input Monitoring 权限问题，中间还误诊过一次：

1. **Input Monitoring 权限缺失（真凶）。** eventtap 要监听键盘事件需要"输入监控"权限（系统设置 → 隐私与安全性 → 输入监控）。坑在于 Hammerspoon 的 `finderTap:isEnabled()` 在这种情况下**仍然返回 true**——tap 对象活着，但事件根本进不来。表现为快捷键静默失效，而所有自检都说自己健康。授权后立即恢复。
2. **"回调抛错会禁 tap"（误诊）。** 当时回调里有一处 AX 调用会抛错，tap 恰好也死了，于是顺理成章把两件事因果到了一起。后来翻 Hammerspoon 源码加实测才发现对不上：回调抛错在 C 层就被捕获，只打一行日志，tap 不受影响。真正的死因从头到尾都是权限。

修复后的脚本做了三层防御：整个回调包在 `pcall` 里（错误不至于吞掉触发按键）；每 5 秒的 watchdog 检查 `isEnabled()` 并复活死掉的 tap；配置头部注释写明需要辅助功能**和**输入监控两项权限——因为权限不足时 `isEnabled()` 会说谎，这是唯一靠谱的提示方式。

排查时还撞上一个方法论陷阱：加逐键日志插桩后连续 100+ 次切换零失败，当时以为是"插桩改变时序"，实际是那次 Hammerspoon 重启恰好让权限问题暂时消失。

### 与 Hyperkey 的关系

如果你也在用 [Hyperkey](https://hyperkey.app)（一个把右 Cmd 映射到「⌘⇧⌥⌃」组合键的轻量工具），不用担心两者冲突：实测 Hyperkey 的映射正常生效的同时，eventtap 收到的仍是裸的 keycode 54 加 `cmd` 标志——两层各自工作，互不干扰。

另一个排查陷阱：从 Hyperkey 的配置读出的键码是 USB HID usage 表的（右 Cmd = 231），不是 macOS CGEvent 键码表的（右 Cmd = 54）。拿 HID 码去查 CGEvent 表会对不上号，别被带偏。

## 小结

一个百来行的脚本，把 macOS 一个存在了十余年、且官方不给改的键位冲突，在事件层解决了——左 Cmd 切标签，右 Cmd 保留原生切视图，两边都不丢。比方案本身更想分享的是三条经验：

- **`isEnabled()` 不代表真的在收事件**。权限缺失时它照样返回 true；验证 tap 健康的唯一办法是按真实键看回调有没有进来；
- **合成事件有两条通道，语义完全不同**。全局 `post()` 会重新流回 tap 链（自己的回调会再看到它），`post(app)` 则绕过 tap 直达应用——选错通道，要么自触发循环，要么事件石沉大海；
- **调试插桩本身会改变被调试系统**——我第一次把日志写进 `~/.hammerspoon/`，触发自动重载的 pathwatcher，配置每秒重载十次，制造了一个全新的 bug。观测工具影响被观测对象，在 GUI 事件层同样成立。

外加一条后来复盘才补上的：**听起来最顺的机制解释也要拿源码和实验验一遍**——"回调抛错禁掉 tap"这个叙事合理到我写了半篇文章，实测根本不成立。
