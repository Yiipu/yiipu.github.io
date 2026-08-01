---
title: "记一次 agentmemory embedding 故障：8 次 hack 全败，一次 GitHub 搜索解决"
date: 2026-08-01 18:00:00 +0800
last_modified_at: 2026-08-01 18:00:00 +0800
categories: [AI, Agent]
description: agent 基础设施出问题时，本能反应是 hack 源码、接旁路。但这个本能是错的。
---

## 背景

agentmemory 是我本地 agent 体系的核心记忆服务，跑在 macOS Apple Silicon 上。某天发现它的向量搜索挂了--embedding 模块死循环，CPU 400%+，REST API 无响应。

降级为 BM25-only（纯关键词搜索），语义搜索完全不可用。

## 根因

agentmemory v0.9.24 用 `@xenova/transformers` 2.17.2 做本地 embedding，模型 `Xenova/all-MiniLM-L6-v2`（384 维，ONNX 量化版）。这个包硬依赖 `onnxruntime-node` 1.14.0，在 Apple Silicon 上跑量化模型的 `DynamicQuantizeMatMul` 时，多线程 NEON 内核死循环。

版本兼容性死锁：

```
onnxruntime-node 1.14.0  -> NEON 死循环（多线程触发）
onnxruntime-node 1.15.0+ -> NEON 修了，但 Tensor API 变了，xenova 2.17.2 不兼容
onnxruntime-node 1.20.0+ -> API 又变，仍然不兼容
```

`@xenova/transformers` 已更名为 `@huggingface/transformers`（v4+），支持新版 onnxruntime。但 agentmemory 硬编码了 `@xenova/transformers` import。

## 8 次 hack，全败

接下来是一个经典的"接旁路"循环。每次尝试看起来都比上一次更聪明，但全部失败：

```
1. 换 onnxruntime-node 1.19.2         -> Tensor.data must be a typed array
2. 换 onnxruntime-node 1.26.0         -> Tensor.location must be a string
3. 删 xenova 内嵌的 onnxruntime-node   -> 顶层版本不匹配，not a valid backend
4. onnxruntime-common 降级到 1.14.0    -> 加载成功，但 NEON 死循环依旧
5. OMP_NUM_THREADS=1 / ORT_NUM_THREADS=1 -> 原生线程池不受控
6. patch backend.js 注入 intraOpNumThreads:1 -> C++ binding 不读这个选项
7. patch wasm.numThreads=1            -> 只影响 WASM 后端，不影响原生
8. 禁用原生后端强制走 WASM            -> no available backend found
```

每个方案都有合理的逻辑推导，每一步都基于上一步的失败深入分析。8 个方案排除了所有"换版本 + 降线程"的技术路径。

但方向本身就是错的。

## 方法论纠偏

在准备第 9 个 hack 的时候，我说了一段话：

> 各种 agent 基础设施软件都在快速迭代阶段，出现问题是必然的。
> 本地 agent 往往倾向于接旁路、改源码等很 hack 的解决方案。
> hack 很酷，很直接，但不适合这种场景--agent 基础设施出了问题，不是传统软件。
> 更好的思路：先查上游 issue、pr、release，然后要么等上游修并发布正式，要么提前把上游的 pr 合进来。

这段话的核心判断：**agent 基础设施迭代极快，你遇到的问题大概率别人也遇到了，维护者很可能已经在修。** 在 node_modules 里 patch 编译产物，是在和一个你无法控制的快速移动目标搏斗。

传统软件（数据库、操作系统、编译器）迭代慢，bug 可能存续多年，hack 有其合理性。但 agent 工具链的迭代周期是周级别的--maintainer 面对几十上百个 PR，你遇到的问题可能已经在某个 PR 里解决了。

## 一次 GitHub 搜索

```
搜索: agentmemory rohitg00 github issue onnxruntime xenova embedding Apple Silicon NEON
```

第一个结果就是 agentmemory 主仓库的最新 commit：

```
chore(deps): migrate @xenova/transformers to @huggingface/transformers v4 (#1096)

@xenova/transformers@2.x is deprecated and silently broken on Node 22+
(see #479). The project was renamed to @huggingface/transformers; same
Apache-2.0 license, same code. v4 ships onnxruntime-node/web and sharp
as hard deps.

Verified: 1424/1424 tests pass, build clean.
CI Node matrix: [20, 22] -> [20, 22, 24, 26] across ubuntu/macos
```

3 天前合入。维护者自己做了方向 A 想做的事，而且做得更彻底：不仅换了包名，还修正了 dtype 默认值（v4 默认 fp32，不显式传 `dtype:"q8"` 会静默回归为 3.5x 下载量 + 更慢推理），加了测试覆盖，扩展了 CI 矩阵。

唯一的问题是：**还没发版。** main 比 v0.9.28 领先 3 个 commit，修复在 main 上但 npm 包还没更新。

## 临时方案：Ollama embedding

上游修复还在路上，但 BM25-only 已经跑了几个月，不能继续等。翻 agentmemory 源码，发现它原生支持 embedding 和 chat 分离的 base URL：

```
OPENAI_EMBEDDING_BASE_URL  - embedding 专用 base URL（默认复用 OPENAI_BASE_URL）
OPENAI_EMBEDDING_API_KEY   - embedding 专用 API key
OPENAI_EMBEDDING_MODEL     - 模型名
OPENAI_EMBEDDING_DIMENSIONS - 维度（非内置模型必须设）
```

源码注释直接举了 Ollama 的例子。本地已有 Ollama + `Qwen3-Embedding-0.6B-GGUF`（1024 维），4 行配置搞定：

```bash
# ~/.agentmemory/.env
EMBEDDING_PROVIDER=openai
OPENAI_EMBEDDING_BASE_URL=http://localhost:11434/v1
OPENAI_EMBEDDING_API_KEY=ollama
OPENAI_EMBEDDING_MODEL=modelscope.cn/Qwen/Qwen3-Embedding-0.6B-GGUF
OPENAI_EMBEDDING_DIMENSIONS=1024
```

Ollama 是独立进程，不阻塞 Node 事件循环。每 batch 32 条，~2s/batch，通过 OpenAI 兼容的 `/v1/embeddings` 端点通信。零 patch，零旁路。

## 索引回填

配好 Ollama 后遇到第二个坑：`DROP_STALE_INDEX=true` 只丢弃向量索引，不回填旧数据。源码逻辑：

```javascript
// src-B8J9Exum.mjs:19677
if (bm25Index.size === 0) rebuildIndex(kv)  // 只有 BM25 空了才完整重建
```

`rebuildIndex()` 同时重建 BM25 + 向量索引。但旧 BM25 索引还在（与 embedding 维度无关），所以条件不满足，重建不触发。结果：旧数据有 BM25（关键词搜索可用），但向量索引为空（语义搜索缺失）。

解决方案：手动删 BM25 索引文件 + 重启，强制触发完整重建：

```bash
rm -f ~/.agentmemory/data/state_store.db/mem%3Aindex%3Abm25*.bin
launchctl kickstart -k gui/$(id -u)/com.agentmemory.server
```

实测：212 个 observations，48 个 embedding 请求，~90s 完成重建。重建后 `DROP_STALE_INDEX` 改回 `false`。

## 时序

```
2026-05-29  agentmemory v0.9.24 发布（当前安装版本）
2026-06-02  embedding 首次出问题，开始排查
2026-06-07  v0.9.27 发布
2026-06-19  v0.9.28 发布（不含修复）
2026-07-29  PR #1096 合入 main（xenova -> huggingface/transformers v4）
2026-08-01  本文：查上游 -> Ollama 临时方案 -> 索引回填
????        v0.9.29 发布（预计包含 PR #1096）
```

## 后续

等 v0.9.29 发布后：

```bash
npm install -g @agentmemory/agentmemory  # 升级
# 删 .env 中的 Ollama embedding 配置，改回 EMBEDDING_PROVIDER=local
# DROP_STALE_INDEX=true 重启一次（1024 -> 384 维度变），再改回 false
```

之前在 node_modules 里的 hack patches 不用清理，`npm install -g` 会覆盖整个目录。

## 总结

```
              本能路径              正确路径
发现问题  ->  读源码找根因       ->  搜上游 issue/PR
          ->  patch 编译产物      ->  发现维护者已修
          ->  排除一种可能        ->  等发版 or 提前合入
          ->  换另一种 patch      ->  临时方案用配置解决
          ->  ...8 次循环         ->  1 次搜索解决
```

不是说 hack 不对。传统软件、你自己的代码、没有上游的东西，hack 是合理的。但对于快速迭代的 agent 基础设施：

1. **先查上游** -- issue、PR、release notes，一般 issue 就够
2. **等上游修** -- 维护者面对几十上百个 PR，你遇到的问题大概率已有 PR
3. **提前合入** -- 如果等不及，cherry-pick 上游 PR 到本地，不要自己从头 patch
4. **配置优于 patch** -- 临时方案优先用产品自带的配置选项（如 `OPENAI_EMBEDDING_BASE_URL`），不要改源码

8 次 hack 花了几个小时，一次 GitHub 搜索花了 10 秒。差的不是技术能力，是第一步的方向选择。
