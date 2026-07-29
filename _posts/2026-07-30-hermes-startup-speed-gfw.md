---
title: Hermes Agent 启动加速：GFW 环境从 37 秒到 7 秒
date: 2026-07-30 00:30:00 +0800
last_modified_at: 2026-07-30 00:30:00 +0800
categories: [工具, Hermes]
description: 在国内网络环境下，Hermes Agent 每次启动耗时 37 秒，排查发现是两个网络超时导致，修复后降至 7 秒。
---

## 问题

Hermes Agent（Nous Research 开源的 AI Agent 框架）在国内网络下启动极慢：

```bash
$ time hermes chat -q "hi" --quiet
real  0m37s
```

一个简单的 `hi` 要等 37 秒，难以忍受。

## 排查

开 verbose 日志：

```bash
$ hermes chat -q "hi" -v
```

关键输出：

```
23:44:10 - OpenAI client created
23:44:10 - Tool checks start
23:44:40 - Failed to fetch models.dev: Connection to models.dev timed out
          (connect timeout=15s)
23:44:40 - Loaded models.dev from disk cache (172 providers)
```

**30 秒空白**之后才报超时。根因是 `agent/models_dev.py` 中的 `fetch_models_dev()` 每次启动尝试请求 `https://models.dev/api.json`，超时设为 15 秒。GFW 阻断该域名，拖满超时后才走本地缓存。

另外 `model_catalog` 默认会拉取所有 provider（包括 copilot、openrouter、anthropic、nous 等国内不可达的），进一步拖慢模型切换。

## 修复

### 1. 让 models.dev 磁盘缓存保持"新鲜"

代码逻辑：如果 `~/.hermes/models_dev_cache.json` 的 mtime 在 1 小时内，直接读缓存，不发起网络请求。

手动刷新一次：
```bash
touch ~/AppData/Local/hermes/models_dev_cache.json
```

然后在 Hermes 里建一个 cron job，每 30 分钟自动刷新 mtime（`no_agent=true`，纯脚本）：

```python
# ~/AppData/Local/hermes/scripts/touch_models_dev_cache.py
import os
p = os.path.expanduser("~/AppData/Local/hermes/models_dev_cache.json")
if os.path.exists(p):
    os.utime(p, None)
```

```bash
hermes cron create "every 30m" \
  --script touch_models_dev_cache.py \
  --no-agent \
  --deliver local
```

### 2. 排除不可达的 provider

编辑 `config.yaml`，加入：

```yaml
model_catalog:
  excluded_providers:
    - copilot
    - openrouter
    - anthropic
    - nous
```

注意：`hermes config set` 会把数组存成 YAML 字符串而非列表，建议直接编辑配置文件。

## 效果

| 操作 | 修复前 | 修复后 |
|------|--------|--------|
| `hermes chat -q "hi"` | 37s | 7s |

5 倍加速，回到正常水平。

## 一个小坑

`model_catalog.enabled: false` 并不能阻止 `models.dev` 的网络请求——这是两套独立的系统。真正跳过网络请求的条件是磁盘缓存的 mtime 在 1 小时内。

## 适用场景

所有在国内网络环境下使用 Hermes Agent 的用户。如果未来需要用到 copilot / openrouter / anthropic / nous 等 provider，从 `excluded_providers` 列表中移除即可。
