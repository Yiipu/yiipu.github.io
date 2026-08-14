---
title: Deepseek Harness 仓库分析报告
date: 2026-08-14 09:00:00 +0800
categories:
  - AI
  - Agent
tags:
  - Deepseek Harness
  - 分析报告
  - 源码分析
description: 对 deepseek-ai/deepseek-harness - 基于 Cordis 的 plugin-first agent harness 的开源仓库分析报告
---

## Executive Summary

DeepSeek Harness (`dsh`) 是一个基于 Cordis 框架的 plugin-first agent harness。它的核心 architectural idea 是：**将 agent 运行时的每一个能力（model adapter、tool registry、session log、agent loop 本身）都建模为 Cordis 插件，通过声明式配置组合而非硬编码依赖来组装一个完整的 agent 运行时。**

与普通 agent framework 的核心区别在于：

1. **没有特权内核**——agent loop、LLM 适配器、工具系统都是可替换的插件，通过 `ctx.<service>` 服务注册表发现彼此
2. **事件溯源会话**——Session 是一个只追加的事件日志，LLM 消息历史从日志派生而非直接存储，使 replay/fork/持久化成为结构性保证
3. **Capability Seam 三角色**——每个可交换能力显式拆分为 Service Definition / Service Provider / Consumer 三个包，使 provider 替换不影响 model-facing 契约

**最值得借鉴的三个设计：**

1. 事件溯源 Session + derived message history——log IS state，replay/fork/persistence 不需要额外机制
2. Patch-layer 配置组合——bundle → profile → user patch → `--patch` overlay 的分层组合，所有配置是可 diff 的 YAML patch
3. Capability Seam 三角色——将 contract/implementation/consumer 显式拆分为独立演化的包边界

**最大的工程成本：**

约 200K 行 TypeScript、~89 个 `ctx.<service>` 声明、107 个 `declare module` 类型扩展，以及 vendored Cordis 框架的 18 处本地修改。系统的学习曲线和调试成本主要来自 Cordis 的 Proxy-based Context、Fiber 生命周期、isolate scope 和 waterfall 事件语义。

**哪些设计依赖 Cordis：**

plugin lifecycle、dependency injection (inject)、scope isolation、event dispatch (waterfall/serial/parallel/emit)、effect disposal、config 驱动加载（Loader/Include/Group）、HMR——这些全部由 vendored Cordis 提供。去掉 Cordis，dsh 剩下的是业务逻辑（agent loop 算法、tool 管道、session 事件模型、sandbox 策略），但失去组合和生命周期管理基础设施。

**如果从零做中等规模 agent harness：**

值得采用：事件溯源 session、patch-layer 配置组合、capability seam 三角色、waterfall 事件扩展点、可逆 effect 注册、durable invariant 检查。

不值得照搬：vendored Cordis 全量（除非你的框架层也需要完全控制）、89 个服务的过度细粒度拆分（对中等规模是过度工程化）、`!!js` YAML 表达式（安全面窄但认知负担高）。

---

## 1. What DeepSeek Harness Actually Is

DeepSeek Harness 是 DeepSeek AI 开发的开源 agent harness，命令行工具 `dsh`。当前版本 0.1.0-rc.5（developer preview），仓库有 1587 个 commit、~200K 行 TS 代码、692 个测试文件。

**README 声称：**

> "It uses an architecture where **everything is a plugin**, and is powered by Cordis."

**源码验证：** Confirmed。`dsh-base` bundle 的 `cordis.patch.yml` 以 `insert` 行的形式声明了约 60 个插件（`timer`、`hmr`、`llm`、`session`、`agent`、`sandbox`、`tools`、`bash`、`fs`、`subagent`、`compaction` 等），每个行通过 `name` 字段指向一个 npm 包，通过 `config` 传递配置。agent loop 自身（`@deepseek-ai/dsh-agent-loop`）是其中一个行，可以被 patch 覆盖或禁用。

**但 "everything is a plugin" 需要精确理解：**

- `Context`、`Fiber`、`Registry`、`Events`、`Service`——这些是 Cordis 框架核心，**不是插件**，它们是插件系统本身
- `Loader`、`Include`、`Group`、`HMR`、`Timer`——这些是 Cordis 官方插件，被 vendored 进仓库，在 `boot()` 中作为基础设施挂载
- ~60 个业务插件——这些是 dsh 自己的包，通过 `cordis.patch.yml` 组合

所以更准确的表述是：**dsh 的所有业务能力都是插件，但这些插件运行在 vendored Cordis 框架之上，框架本身不是插件。**

---

## 2. Repository Map

```
deepseek-harness/
├── apps/
│   ├── cli/          — `dsh` 命令行入口 (bin.ts → args.ts → profile-boot.ts)
│   └── web/          — Web UI 前端
├── packages/         — ~89 个业务包 (dsh-*)
│   ├── core/         — agent, agent-loop, session, scope, tools, system-prompt
│   ├── llm/          — llm 核心 + deepseek/pi-ai 适配器 + retry + token-meter
│   ├── shell/        — shell 抽象 + bash/pwsh 工具 + sandbox 变体
│   ├── sandbox/      — sandbox 抽象 + local(windows/linux/darwin) + policy
│   ├── session/      — persistence (jsonl/sqlite) + query + checkpoint
│   ├── storage/      — 通用 KV 存储 (json/sqlite/domain)
│   ├── subagent/     — 子代理 seam + spawn/fork/acp/codex/claude-code providers
│   ├── skill/        — skill seam + filesystem discovery + tool-skill consumer
│   ├── compaction/   — compaction seam + basic engine + tool-result-pruner
│   ├── context/      — agent-instructions + session-reference + time + tmux
│   ├── boot/         — app-boot + cmdline
│   ├── bundle/       — base + headless + web-app (cordis.patch.yml)
│   └── ...           — 44+ 个其他包
├── vendor/           — vendored Cordis 框架 (9 个包)
│   ├── cordis/       — Context, Fiber, Registry, Events, Service, Logger
│   ├── loader/       — 配置驱动加载器
│   ├── include/      — YAML/JSON patch 系统
│   ├── group/        — 嵌套分组
│   ├── hmr/          — 热重载
│   ├── cosmokit/     — 基础工具库
│   ├── schemastery/  — schema 验证 (Standard Schema V1)
│   ├── timer/        — 定时器服务
│   └── logger-console/
├── native/landlock-run/  — Linux Landlock 沙箱 native addon
├── python/           — Python SDK runtime
├── .agents/          — agent-facing infrastructure
│   ├── skills/       — 11 个 agent skill
│   └── notes/        — Agent Notes (类 RFC 决策记录)
├── docs/             — 架构文档 + 教程 + cookbook
└── scripts/          — ~50 个构建/验证脚本
```

### 关键指标

| 指标 | 值 |
|---|---|
| TS 源文件 (packages+vendor+apps) | 2,119 |
| 业务包 TS 代码行 (非测试) | ~199K |
| 测试文件 (.spec.ts) | 692 |
| E2E 测试文件 (.e2e.ts) | 129 |
| `ctx.<service>` 声明 | 89 |
| `declare module '@deepseek-ai/cordis'` 类型扩展 | 107 |
| 使用 `static inject` 的包 | 42 |
| Vendored Cordis 本地修改 | 18 处 |
| Agent Notes (implemented) | ~30+ |

### Entry Points

| 入口 | 文件 | 职责 |
|---|---|---|
| CLI | `apps/cli/src/bin.ts` | 解析 `dsh` 命令，按 mode 分发 |
| Profile boot | `apps/cli/src/profile-boot.ts` | 组合 patch layers → `boot()` → Cordis runtime |
| Plugin management | `apps/cli/src/plugin.ts` | `dsh plugin` → pnpm forwarder + reconcile |
| Config dump | `apps/cli/src/dump-config.ts` | 打印组合后的插件树 |
| Web dev | `apps/web/src/` | Vite + React Web UI |
| Test | `vitest.config.ts` + 5 个变体 | unit/e2e/snapshot/web/perf/stress |
| Programmatic API | Python SDK (`python/sdk/`) | JSON-RPC agent 控制 |

---

## 3. Architecture Reconstruction

### 3.1 Runtime Hierarchy

根据源码重建的真实结构：

```
Process
 └── boot()                           [packages/boot/app-boot/src/index.ts:757]
      ├── new Context()                [vendor/cordis/src/context.ts:71]
      │    └── Proxy<Context>          — ReflectService.handler
      │         ├── ReflectService     — service resolution
      │         ├── RegistryService    — plugin registry
      │         ├── EventsService      — event bus
      │         ├── LoggerService      — logging
      │         └── Fiber (root)       — lifecycle root
      │
      ├── ctx.plugin(Loader)           — config-driven loader service
      ├── prepare?(ctx)                — host preparation (env, cmdline, etc.)
      └── mountRootInclude(ctx, ...)   — Include plugin reads cordis.yml
           └── applyEntryPatches()     — apply bundle + user patch layers
           └── EntryGroup.update()     — transactional entry creation
                └── Entry.init()       — import module → Fiber._reload()
                     └── Plugin callback(ctx, config)
                          ├── ctx.provide('tools', ...)   — Service registration
                          ├── ctx.on('agent/pre-step', ...) — Event listener
                          ├── ctx.effect(...)              — Reversible effect
                          └── ctx.tools.register(...)      — Tool registration
```

**关键发现：** 不存在传统的 "Application → Runtime → Agent" 嵌套。整个运行时就是**一个 Cordis Context**，agent 是在这个 context 中注册的服务实例。agent 和 plugin 是**平级的**——agent loop 自身就是一个插件（`dsh-agent-loop`），它通过 `ctx.agents` 服务创建 agent 实例。

### 3.2 Initialization Lifecycle

追踪 `dsh --profile web` 的完整启动链：

```
1. process start
   └── bin.ts: parseDshArgs() → { mode: 'profile', profile: 'web' }

2. configuration
   └── profile-boot.ts: loadLayeredEnv() → 环境快照
   └── composeProfile('web')
        ├── prepareProfile('web') → loadProfile() → healProfilesModuleFallback()
        ├── bundlePatches: dsh-base + dsh-web-app 的 cordis.patch.yml
        ├── homePatches: ~/.dsh/cordis.patch.yml
        └── overlays: --patch + telemetry switch

3. runtime creation
   └── boot('dsh', rootConfig, patches, prepare)
        ├── new Context() → Cordis root context
        ├── ctx.plugin(Loader) → 加载 Loader 服务
        └── prepare(ctx) → provide env snapshot + cmdline args

4. plugin registration (config-driven)
   └── mountRootInclude() → Include reads empty cordis.yml
        └── applyEntryPatches(data, allPatches) → 组合后的 entry list
        └── EntryGroup.update(config) → transactional create
             └── for each entry:
                  ├── Entry.init() → import(name) → plugin module
                  └── Fiber._reload()
                       ├── _resolveConfig() → schema 验证
                       └── _runner.execute(ctx, config) → plugin callback

5. plugin activation (inject-driven, lazy)
   └── Fiber._refresh() → 检查 inject 依赖
        └── 依赖未就绪 → PENDING (等待)
        └── 依赖就绪 → ACTIVE (执行 callback)

6. service initialization
   └── 各 Service 子类构造 → ctx.reflect.provide(name, self)
   └── ctx.inject([...], callback) → 等待服务就绪后执行

7. agent initialization (on-demand)
   └── ctx.agents.create(options) → AgentFactory.createAgent()
        ├── new Session() → 事件日志
        ├── new ReactLoopAgent() → agent loop driver
        ├── setup callback → 组合 agent scoped world
        └── announce → emit agent/created + session/created

8. execution (event-loop driven)
   └── agent.followup(message) → wakeDriver() → kick() → turn() → step()
        └── LLM stream → tool dispatch → context update → next step

9. teardown
   └── SIGINT/SIGTERM → shutdown.interrupt() → ctx.fiber.dispose()
        └── Fiber._unload() → 逆序执行所有 effect disposers
```

**关键特征：**

- **Eager:** Context、Loader、Include 在 `boot()` 中 eager 加载
- **Lazy:** 业务插件通过 `inject` 声明依赖，依赖未就绪时 PENDING；agent 按需创建
- **Dependency ordering:** 通过 `inject` 声明而非手动排序——`ctx.inject(['llm', 'sessions'], callback)` 在两者都就绪后执行
- **Teardown:** Fiber 逆序执行所有 effect disposers；`installFailLoud` 确保未处理的 rejection 终止进程

### 3.3 Plugin Lifecycle

| 阶段 | 机制 | 证据 |
|---|---|---|
| 声明 | `cordis.patch.yml` 中的 entry（`id` + `name` + `config`） | `packages/bundle/base/cordis.patch.yml` |
| 发现 | `dsh plugin` → pnpm install → `reconcilePlugins()` 检查 `dsh.bundle` 声明 | `apps/cli/src/plugin.ts:50-85` |
| 加载 | `Entry.init()` → `import(name)` → `unwrapExports()` | `vendor/loader/src/config/entry.ts:259` |
| 激活 | `Fiber._reload()` → schema 验证 → 执行 callback | `vendor/cordis/src/fiber.ts:646-673` |
| 获取依赖 | `static inject = ['llm', 'sessions']` → `Fiber._refresh()` 检查 | `vendor/cordis/src/fiber.ts:611-623` |
| 访问 runtime | `ctx.tools`、`ctx.llm` 等——通过 Proxy + ReflectService 解析 | `vendor/cordis/src/reflect.ts:136-171` |
| 暴露能力 | `ctx.provide(name, value)` 或 `Service` 子类构造 | `vendor/cordis/src/reflect.ts:277-305` |
| 监听事件 | `ctx.on('event/name', listener)` → 注册为 fiber effect | `vendor/cordis/src/events.ts:288-302` |
| 销毁 | `ctx.fiber.dispose()` → 逆序执行 effect disposers | `vendor/cordis/src/fiber.ts:675-696` |
| 通信 | 通过 `ctx.emit()` / `ctx.waterfall()` / `ctx.parallel()` 事件 | `vendor/cordis/src/events.ts` |

**Plugin 之间不直接引用**——它们通过 `ctx.<service>` 发现彼此，通过事件通信。这是 "everything is a plugin" 的实际含义：没有 `import { LlmService } from '...'`，只有 `ctx.llm.stream(request)`。

### 3.4 Scope / Context Model

dsh 在 Cordis 的 Context 之上增加了 `dsh-scope`（`packages/core/scope/src/index.ts`）——一个 per-agent 的注册边界。

**为什么需要 scope？**

当一个进程中有多个 agent（父 + 子代理），每个 agent 有自己的工具集、prompt section、事件监听器。如果没有 scope，所有注册都是全局的——子代理的工具会出现在父 agent 的 prompt 中。

**Scope 的实现：**

```typescript
// packages/core/scope/src/index.ts:137
export function createScope(ctx: Context, key: ScopeKey): Scope {
  const fiber = ctx.plugin(scope)  // 创建一个 no-op 插件的 fiber
  const scoped: Context = fiber.ctx.extend({ [kScope]: key })
  return { ctx: scoped, rawDispose: fiber.dispose, dispose: ... }
}
```

- **层级：** scope 通过 `scopeParents` WeakMap 形成父子链
- **继承：** 子 scope 继承父 scope 的注册（`ScopedLayers` 合并读取）
- **事件传播：** 事件沿 scope 链**向上**传播——父 scope 的监听器收到子 scope 的事件
- **隔离：** 子 scope 的注册不影响父 scope（通过 fiber dispose 清理）

**Scope 与 Cordis isolate 的区别：**

- `isolate` 是 Cordis 内置的服务多实例隔离（同一服务名在不同 isolate label 下有不同实现）
- `scope` 是 dsh 在 isolate 之上的 agent 级注册隔离（同一个 `ctx.tools` 服务，但工具注册通过 scope 过滤）

---

## 4. Runtime and Lifecycle

### 4.1 谁做什么

| 职责 | 由谁 | 证据 |
|---|---|---|
| 创建 runtime | `boot()` in app-boot | `packages/boot/app-boot/src/index.ts:757` |
| 创建 context | `new Context()` | `vendor/cordis/src/context.ts:71` |
| 创建 agent/session | `AgentRegistry.create()` | `packages/core/agent/src/index.ts:405` |
| 管理 lifecycle | `Fiber` | `vendor/cordis/src/fiber.ts` |
| 驱动 event loop | Node.js 事件循环（无自定义循环） | — |
| 加载 plugin | `Loader` + `Include` | `vendor/loader/src/`, `vendor/include/src/` |
| 执行 tool | `ToolRuntime` + `TOOL_RUNTIME_SCHEDULER` | `packages/core/tools/src/index.ts` |
| 与 model 交互 | `LlmRuntime` + adapter | `packages/llm/llm/src/index.ts` |

### 4.2 Fiber 状态机

```
PENDING → LOADING → ACTIVE → UNLOADING → DISPOSED
                 ↘ FAILED ↗
```

- **PENDING:** inject 依赖未就绪
- **LOADING:** 依赖就绪，正在执行 plugin callback
- **ACTIVE:** callback 完成，fiber 活跃
- **UNLOADING:** 正在执行 disposers
- **DISPOSED:** 完全销毁
- **FAILED:** 加载失败

关键设计：当 inject 的服务实现变化时（如 HMR 导致 provider 替换），fiber 的 epoch 改变，触发 `_unload()` → `_reload()` 循环。

---

## 5. Plugin Architecture

### 5.1 Plugin 的三种形态

```typescript
// vendor/cordis/src/registry.ts:92-146
type Plugin =
  | Plugin.Function    // function(ctx, config)
  | Plugin.Constructor // class with constructor(ctx, config)
  | Plugin.Object      // { apply(ctx, config) }
```

所有形态共享 `Base` 元数据：`name`、`Config`（schema）、`inject`、`provide`、`intercept`。

### 5.2 配置驱动的组合

dsh 不使用编程式插件注册。所有插件通过 `cordis.patch.yml` 声明：

```yaml
# packages/bundle/base/cordis.patch.yml (简化)
- insert:
    - id: llm
      name: '@deepseek-ai/dsh-llm'
    - id: session
      name: '@deepseek-ai/dsh-session'
    - id: agent-loop
      name: '@deepseek-ai/dsh-agent-loop'
      config:
        agents: []
    - id: sandbox-policy
      name: '@deepseek-ai/dsh-sandbox-policy'
      config:
        mode: !!js process.env.DSH_PERMISSION_MODE ?? 'workspace-write'
```

**Patch 语义：**

- `insert` 添加新行（可带 `id` 目标 group）
- 非 `insert` 按 `id` 匹配并覆盖 `config` / `disabled` 等字段
- 后来的 patch layer 覆盖先前的（last write wins per row）
- `!!js` 表达式在 entry 的 fiber 激活后对其 context 求值

### 5.3 "Everything is a Plugin" 验证

| 能力 | Plugin? | 证据 |
|---|---|---|
| Model adapter | ✅ | `llm-deepseek`、`llm-pi-ai` 行 |
| Tool | ✅ | `tool-bash`、`tool-fs`、`tool-web` 等行 |
| Agent | ✅ | `agent` + `agent-loop` 行 |
| Memory | ❌ | 无 long-term memory system；session log 是唯一持久化 |
| Transport | ✅ | `api-gateway`、`acp` 行 |
| UI | ✅ | `web-app` bundle 挂载 React 前端 |
| Storage | ✅ | `session-persistence-jsonl`、`storage-json` 行 |
| Middleware | ✅ | waterfall 事件就是 middleware |
| Logging | ✅ | `logger-console` 是 vendored plugin |
| Authentication | ✅ | `credentials-local`、`approval`、`permission-presets` 行 |
| Sandbox | ✅ | `sandbox-local`、`sandbox-policy` 行 |
| Compaction | ✅ | `compaction-basic`、`tool-result-pruner` 行 |
| Subagent | ✅ | `subagent`、`subagent-spawn-in-process`、`subagent-fork-in-process` 行 |
| Skill | ✅ | `skill`、`skill-filesystem`、`tool-skill` 行 |

**结论：** Confirmed。dsh 的所有业务能力都建模为插件。唯一不是插件的是 Cordis 框架本身（Context/Fiber/Registry/Events/Service）和 Node.js 运行时。

### 5.4 成本分析

| 成本 | 证据 |
|---|---|
| **Indirection** | 每个服务访问经过 Proxy + ReflectService → `internal/get` waterfall → fiber 链查找 |
| **Debugging difficulty** | Proxy 使 stack trace 难以阅读；effect disposal 是异步的且可能跨 fiber |
| **Lifecycle complexity** | Fiber 状态机有 6 个状态；HMR 触发 unload→reload 循环；reentrant disposal 需要 18 处本地加固 |
| **Dependency ambiguity** | `inject` 声明的是服务名而非包名——同名服务可被 isolate 分隔 |
| **Cognitive load** | 需要理解 Context proxy、Fiber lifecycle、isolate scope、waterfall 语义、patch 组合、`!!js` 表达式求值时机 |

---

## 6. Scope / Context / Dependency Model

### 6.1 为什么需要 scope

Agent harness 的核心挑战：**一个进程中运行多个 agent，每个 agent 有自己的工具集和状态。**

dsh 的解决方案是在 Cordis 的 Context proxy 之上增加 scope 层：

1. **`createScope(ctx, agent)`** → 创建一个带 scope tag 的子 context
2. **`ctx.tools.register(definition)`** → 通过 scope layer 过滤，nearest scope 优先
3. **`scopeTarget(agent, agent)`** → 构建 event carrier，事件沿 scope 链向上传播

### 6.2 Scope 与 isolate 的协作

| 机制 | 层 | 用途 |
|---|---|---|
| Cordis `isolate` | 框架 | 服务多实例隔离（同一服务名在不同 label 下有不同实现） |
| dsh `scope` | 业务 | Agent 级注册隔离（同一服务，不同注册集） |
| Cordis `extend` | 框架 | Context 原型链继承（添加 metadata） |
| Cordis `intercept` | 框架 | 服务配置拦截（merge config） |

---

## 7. Agent Execution Path

### 7.1 完整调用链

```
User Input
 → agent.followup(message)                    [packages/core/agent-loop/src/agent.ts:122]
 → inbox.splice('next-turn', ...)              [agent.ts:118]
 → wakeDriver()                                [agent.ts:172]
 → kick()                                      [agent.ts:210]
   while (await turn()) {}                     [agent.ts:212]
   
   → turn()                                    [agent.ts:246]
     ├── session.append('turn/start')          [agent.ts:255]
     └── while (true)                          [agent.ts:263]
       → preStep(target, {turn, step})         [agent.ts:225]
         ├── inbox.claim(target, turn)         [agent.ts:229]
         ├── systemPrompt.assemble(...)        [agent.ts:230]
         ├── runtimeContext.project(...)       [agent.ts:233]
         └── dispatch.waterfall('agent/pre-step', ...)  [agent.ts:234]
           → listeners may reject or rewrite messages
       
       → step(assembly)                        [agent.ts:332]
         ├── renderPrompt(assembly)            [agent.ts:337]
         ├── buildRequest(turn, step, ...)     [agent.ts:407]
         │   ├── session.deriveMessages()      [agent.ts:341]
         │   ├── dispatch.waterfall('agent/request', ...)  [agent.ts:438]
         │   └── ctx.llm.prepareCall(config)   [agent.ts:449]
         │
         ├── preparedCall.stream(request)      [agent.ts:345]
         │   └── for await (chunk of stream)   [agent.ts:347]
         │       └── session.append('assistant/chunk', ...)  [agent.ts:349]
         │
         ├── assembler.finish                  [agent.ts:353]
         │   ├── error/aborted → waterfall('agent/request-error', ...)
         │   └── max-tokens → return
         │
         ├── session.append('assistant/message', ...)  [agent.ts:381]
         │
         ├── toolCalls = message.content.filter(tool-call)  [agent.ts:393]
         │   └── if empty → return { kind: 'completed' }
         │
         └── executeToolCalls(ctx, turn, step, toolCalls, signal, ...)  [agent.ts:395]
             → packages/core/agent-loop/src/tool-calls.ts:59
             ├── ctx.agents.requireInitiator()  [tool-calls.ts:67]
             ├── for each tool call group:
             │   ├── ctx.tools.executionMode(exec)  [tool-calls.ts:88]
             │   ├── runGroup(...)                   [tool-calls.ts:121]
             │   │   ├── ctx.tools[TOOL_RUNTIME_SCHEDULER].prepare(exec)  [tool-calls.ts:169]
             │   │   │   → tools/pre-execute waterfall (allow/deny/ask)
             │   │   ├── ctx.tools[TOOL_RUNTIME_SCHEDULER].dispatch(exec)  [tool-calls.ts:173]
             │   │   │   → tools/execute waterfall (timeout/retry/metrics)
             │   │   │   → tool.execute(args, exec)
             │   │   └── ctx.tools[TOOL_RUNTIME_SCHEDULER].finalize(exec, result)  [tool-calls.ts:152]
             │   │       → tools/post-execute waterfall (accept/block/replace)
             │   └── session.append('tool/result', ...)  [tool-calls.ts:281]
             │
             └── return { concluded }
         
         → if concluded → return { kind: 'completed' }
         → else → return null (continue to next step)
       
       → if turnEnds && no next-step input → break
       → target = 'next-step'
     
     → session.append('turn/end', { turn, reason })  [agent.ts:319]
     → if !inbox.hasPending → return false (stop driver)
     → else → return true (next turn)
```

### 7.2 Agent Loop 在哪里

**Confirmed：** 存在一个显式的 `while` loop——`ReactLoopAgent.kick()` 中的 `while (await this.turn()) {}`（`agent.ts:212`）。agent loop 没有被拆散到 event system 中。

但 loop 的**扩展点**确实通过事件暴露：

- `agent/pre-step` (waterfall) — 修改/拒绝即将进入 step 的消息
- `agent/request` (waterfall) — 修改 LLM 请求配置
- `llm/stream` (waterfall) — 包裹 LLM 流式调用
- `agent/request-error` (waterfall) — 处理 LLM 错误
- `agent/turn-stopping` (serial) — turn 即将停止时通知
- `tools/pre-execute` (waterfall) — 工具执行前策略
- `tools/execute` (waterfall) — 工具执行包裹
- `tools/post-execute` (waterfall) — 工具结果后处理

### 7.3 关键设计：Turn / Step 边界

- **Turn** = 零或多个 step，从第一条输入被 claim 开始，到没有待处理输入为止
- **Step** = 一次 LLM 请求 + 其工具调用
- **Inbox** 有两个队列：`next-turn`（下一 turn）和 `next-step`（当前 turn 的下一步）

这个设计允许：
- **Steering**：`agent.steer(message)` → `next-step` 队列，在当前 turn 的下一步注入
- **Followup**：`agent.followup(message)` → `next-turn` 队列，开新 turn
- **Inject**：`agent.inject(message)` → `next-step` 队列但不唤醒（等待其他消息唤醒）

---

## 8. Cordis: What It Provides

### 8.1 Cordis 职责边界

| 机制 | 由 Cordis 提供 | 由 dsh 构建 |
|---|---|---|
| Plugin lifecycle | ✅ Fiber 状态机 | — |
| Dependency injection | ✅ `inject` + `_refresh()` | — |
| Service registry | ✅ `ctx.provide()` + Proxy | — |
| Event bus | ✅ 5 种 dispatch mode | — |
| Effect disposal | ✅ `ctx.effect()` | — |
| Scope isolation | ✅ `isolate()` | dsh 在其上构建 `scope` |
| Config loading | ✅ Loader + Include | — |
| HMR | ✅ chokidar + partialReload | — |
| Schema validation | ✅ schemastery | — |
| Agent loop | — | ✅ ReactLoopAgent |
| Tool pipeline | — | ✅ ToolRuntime |
| Session log | — | ✅ Session (event sourcing) |
| LLM streaming | — | ✅ LlmRuntime + adapters |
| Sandbox | — | ✅ SandboxProvider |
| Persistence | — | ✅ SessionPersistence |

### 8.2 如果去掉 Cordis

去掉 Cordis，dsh 剩下的是：

1. **Agent loop 算法**（turn/step/inbox 驱动）——可移植到任何 event loop
2. **Session 事件模型**（SessionEventMap + deriveMessages）——可独立实现
3. **Tool 管道**（prepare → dispatch → finalize）——可独立实现
4. **Sandbox 策略**（mode + escalation）——可独立实现
5. **业务逻辑**（compaction、subagent、skill 等）——可独立实现

但失去：

1. **Plugin 组合**——没有配置驱动的声明式组合
2. **生命周期管理**——没有自动的 effect disposal
3. **服务发现**——没有 Proxy-based `ctx.<service>` 解析
4. **HMR**——没有模块级热重载
5. **多 agent 隔离**——没有 isolate/scope

**结论：** Cordis 是 dsh 的架构骨架。dsh 的创新不在框架层（Cordis 来自 cordiverse 社区），而在业务层——特别是事件溯源 session、capability seam 三角色、patch-layer 组合和 tool 管道设计。

---

## 9. State, Context and Persistence

### 9.1 状态分类

| 状态 | Owner | Lifetime | Mutable? | Persistent? | Storage |
|---|---|---|---|---|---|
| Process state | Node.js | Process | Yes | No | — |
| Runtime state | Cordis Context | Runtime | Yes | No | — |
| Plugin state | Fiber | Plugin lifetime | Yes | No | — |
| Session state (events) | Session | Session lifetime | Append-only | Yes | JSONL / SQLite |
| Agent state (phase) | ReactLoopAgent | Agent lifetime | Yes | No | — |
| Conversation state | Derived from session | — | No | Yes (via events) | — |
| Tool state | ToolRunContext | Tool call | No (frozen) | No | — |
| Configuration | cordis.patch.yml | Static | Via HMR | Yes | YAML file |
| Sandbox mode | Session event | Session | Append-only | Yes | JSONL / SQLite |
| Settings | settings.yaml | Static | Hot-reloaded | Yes | YAML file |
| Credentials | .credentials.yaml | Static | Per-request | Yes | YAML file |

### 9.2 核心设计：Session 是唯一持久化状态

dsh **没有 long-term memory system**。Session log（事件日志）是唯一的持久化状态：

- **LLM 消息历史**从 session events 派生（`deriveMessages()`）
- **Sandbox mode override**作为 `sandbox/mode` 事件记录在 session log 中
- **Compaction**通过 `surfaceOp: replace` 修改派生历史，但不修改原始日志
- **Replay/Fork** = 用现有日志种子新 session

**"Model-visible means logged"** 是运行时 invariant——任何到达 LLM 请求的内容必须可从 session log 重建。这个 invariant 由 `packages/core/agent-loop/src/invariant.ts` 中的 `llm/stream` 监听器强制检查。

### 9.3 持久化架构

```
Session.append(event)  — 同步追加到内存日志
  → emit('session/event')  — 同步通知
    → PersistenceCoordinator.enqueue()  — 异步缓冲
      → SessionWriteBehind  — 200ms 批处理窗口
        → backend.appendBatch()  — 持久化到 JSONL / SQLite

session/flush  — 持久性检查点（await）
  → 在 LLM 请求前、工具分发前、step 开始前触发
  → 失败是 fail-closed 的（下游不被调用）
```

两个后端：
- **JSONL**：每会话一个文件，Zstandard 压缩，chunk 打包，撕裂恢复用字节偏移
- **SQLite**：WAL 模式，FTS5 全文搜索，撕裂恢复用 seq 删除点

---

## 10. Agent-facing Infrastructure

### 10.1 仓库自身的 agent 工作流

dsh 仓库有一套完善的 agent-facing infrastructure：

| 设施 | 位置 | 用途 |
|---|---|---|
| `AGENTS.md` | 根 + packages/ + docs/ + .github/ | 多层级 agent 指令（CLAUDE.md 是 symlink） |
| `.agents/skills/` | 11 个 skill 目录 | 可复用 agent 工作流（code review、pre-push checks 等） |
| `.agents/notes/` | proposed/implemented/rejected/archived | 类 RFC 决策记录（机器检查格式） |
| `.claude/skills/` | symlink → `.agents/skills/` | Claude Code 兼容 |

### 10.2 Agent Notes 作为决策基础设施

Agent Notes 不是普通文档——它们是**机器检查的决策记录**：

- 格式由 `verify-agent-note-format` 脚本强制（头部三行 + `## Problem` 开头 + `Alternatives considered` 强制）
- 归档由 `verify-archived-agent-notes` 强制（冻结、不可编辑）
- 每个非平凡变更必须在同一 PR 中添加或更新 Agent Note

代表性 notes：
- `2026-06-11-event-sourced-sessions.md` — 事件溯源会话决策
- `2026-06-13-capability-seams.md` — Capability Seam 三角色
- `2026-06-21-subagent-capability-seam.md` — 子代理能力接缝

### 10.3 与运行时架构的关联

**Confirmed：** agent-facing infrastructure 与运行时架构是解耦的。`.agents/skills/` 和 `AGENTS.md` 是开发时工具，不参与运行时。但 dsh 的 **skill 系统**（`packages/skill/`）将 `.agents/skills/` 作为运行时 skill 发现源之一——project-level `.dsh/skills/` 和 `.agents/skills/` 都会被 `skill-filesystem` 插件发现并注册到 `ctx.skills`。

---

## 11. Engineering Highlights

### Highlight: Event-Sourced Session with Derived Message History

**Problem:** Agent 需要严格可回放的 trace——replay、fork、持久化、telemetry 都依赖完整的对话历史。

**Design:** Session 是一个只追加的 `SessionEvent` 日志。LLM 消息历史通过 `deriveMessages()` 从日志派生，而非直接存储。

**Implementation:** `packages/core/session/src/index.ts` — `Session` 类维护 `log: SessionEvent[]`，`SurfaceManager` 按序投影产生 `Message[]`。只有 `user/message`、`assistant/message`、`tool/result` 是 surface 事件（产生 LLM 消息）。`surfaceOp: 'replace'` 允许 compaction 替换一段历史。

**Why it works:** 日志是不可变真相源。派生是纯函数，带缓存。replay/fork 只需复制日志。持久化只需追加写入。

**Reusable lesson:** 任何需要 replay/fork/audit 的系统都应考虑事件溯源。关键是区分"事实"（事件）和"视图"（派生状态）。

**Cost / limitation:** 派生成本随日志长度增长——compaction 是必要缓解。`deriveMessages()` 有缓存但首次派生仍需遍历全日志。

### Highlight: Patch-Layer Configuration Composition

**Problem:** Agent harness 需要在不同部署（web/headless/custom）、不同平台（Linux/macOS/Windows）、不同用户偏好下组合不同的插件集，且组合必须可 diff、可审计、可热重载。

**Design:** 所有配置以 YAML patch layer 形式声明。组合顺序：bundle layers → profile `cordis.patch.yml` → home `cordis.patch.yml` → `--patch` overlays。每个 patch 可 insert 新行或覆盖已存在行的 config。

**Implementation:** `packages/boot/app-boot/src/index.ts:mountRootInclude()` → `applyEntryPatches()` → `EntryGroup.update()`。HMR 监听 patch 文件变更并事务性刷新。

**Why it works:** Patch 是声明式的——用户不需要编程就能组合。`dsh --dump-config` 打印最终组合树。`!!js` 表达式允许环境条件化（如 `disabled: !!js process.platform === 'win32'`）。

**Reusable lesson:** 配置组合应该是分层的 patch 而非单文件。每个层只关心自己的增量。`dsh --dump-config` 式的"打印最终配置"能力对调试至关重要。

**Cost / limitation:** `!!js` 表达式是 `eval` 的安全面——虽然限制了 bootstrap-only 变量，但仍是代码注入入口。Patch 理解需要理解整个组合栈。

### Highlight: Capability Seam Three-Role Split

**Problem:** 可交换能力（如 bash 执行器：local/sandbox/remote）的 contract、implementation 和 consumer 以不同速率变化。如果绑定在一个包中，替换 implementation 会 churn consumer 的 model-facing 契约。

**Design:** 每个可交换能力显式拆分为三个角色：
1. **Service Definition** — Cordis `Service` 子类 + vocabulary 类型（如 `dsh-shell`: `ShellExecutor`）
2. **Service Provider** — 实现接口的插件（如 `dsh-bash-local`、`dsh-bash-sandbox`）
3. **Consumer** — model-facing 工具（如 `dsh-tool-bash`）

**Implementation:** bash trio: `packages/shell/shell/` (definition) + `packages/shell/bash-local/` + `packages/shell/bash-sandbox/` (providers) + `packages/shell/tool-bash/` (consumer)。Consumer 通过 `inject: ['shell']` 声明依赖，不 import provider 类型。

**Why it works:** Provider 替换是配置变更（替换 `cordis.patch.yml` 中的行），不触及 consumer 代码。Model-facing 契约（tool schema）独立版本化。

**Reusable lesson:** 当一个能力有多个可能的实现时，将 contract/implementation/consumer 拆分为独立的包/模块。但**不要提前拆分**——只有一个 conceivable provider 时保持一个包。

**Cost / limitation:** 增加包数量和 boilerplate（package.json、tsconfig、README、injection wiring）。dsh 有 89 个包，部分原因来自这个模式。

### Highlight: Waterfall as Universal Extension Point

**Problem:** Agent loop 的每一步（pre-step、request、LLM stream、tool pre-execute、tool execute、tool post-execute）都可能需要被 plugin 拦截、修改或否决。

**Design:** 使用 Cordis 的 `waterfall` dispatch mode 作为 around-middleware。每个扩展点是一个 waterfall 事件，listener 收到 `(...args, next)` 并调用 `next()` 委托。不调 `next()` 则否决（short-circuit）。

**Implementation:** `agent/pre-step`、`agent/request`、`llm/stream`、`tools/pre-execute`、`tools/execute`、`tools/post-execute` 都是 waterfall。例如 `agent/pre-step` 的 listener 可以 reject 整个 step 或 rewrite 消息。

**Why it works:** Waterfall 是声明式的——plugin 只声明它关心的事件，不需要修改 loop 代码。`prepend: true` 允许优先级控制。Scope 过滤确保 agent-scoped listener 只收到该 agent 的事件。

**Reusable lesson:** Agent loop 的每个可扩展步骤都应暴露为 around-middleware 事件。`next()` 委托是比返回值更清晰的否决语义。

**Cost / limitation:** Waterfall 链是线性的——listener 按注册顺序执行，不能并行。长 waterfall 链增加延迟。调试困难（需要追踪整个链）。

### Highlight: Durable Invariant Checking

**Problem:** "Model-visible means logged" 是核心 invariant，但如何确保它在运行时始终成立？

**Design:** 通过 `llm/stream` 事件的 `prepend: true` 监听器，在每次 LLM 请求前验证：
1. 请求对象已 frozen
2. 携带 session id
3. Session 存活
4. Messages 数组已 frozen
5. Session log 有 `step/start`
6. Messages 与 `deriveMessages()` 一致
7. 请求 header 与 folded header 一致

**Implementation:** `packages/core/agent-loop/src/invariant.ts` — 注册为 `invariants` 服务的 companion plugin。

**Why it works:** Invariant 在**实际 LLM 调用前**检查，不是事后审计。`prepend: true` 确保它先于可能 short-circuit 的 replay listener。

**Reusable lesson:** 核心 invariant 应该在运行时强制检查，而非仅靠代码审查。将 invariant 检查注册为事件监听器，在关键操作前执行。

**Cost / limitation:** 每次 LLM 请求都有 `JSON.stringify` 比较开销。Invariant 失败是 hard error（`fail()` 抛出），在生产中可能过于严格。

---

## 12. Reusable Patterns

### Pattern: Event-Sourced Agent Session

**Context:** 任何需要 replay/fork/audit 的 agent 系统。

**Problem:** 对话状态需要是可重建的——crash recovery、session fork、telemetry 都依赖完整历史。

**Mechanism:** Session 是只追加事件日志。消息历史通过有序 surface 投影派生。只有特定事件类型产生 LLM 消息。`surfaceOp` 支持 `append` 和 `replace`（compaction）。

**Consequence:** Replay/fork 是结构保证。持久化是追加写入。但派生成本随日志增长，需要 compaction。

**When to use:** 任何需要 replay/fork/audit 的系统。
**When NOT to use:** 纯无状态 API 或不需要历史回放的简单 chatbot。

**Example in dsh:** `packages/core/session/src/index.ts` — `Session` 类 + `SurfaceManager`

### Pattern: Patch-Layer Plugin Composition

**Context:** 需要在不同部署/平台/用户偏好下组合不同插件集的系统。

**Problem:** 插件组合不能硬编码，但也不能要求用户编程。

**Mechanism:** 所有插件以 YAML entry 声明。组合是分层的 patch：base bundle → mode bundle → profile patch → home patch → CLI overlay。Patch 可 insert 新行或覆盖已存在行的 config。`dsh --dump-config` 打印最终组合。

**Consequence:** 组合是声明式且可 diff 的。但 `!!js` 表达式是 eval 安全面，patch 理解需要全局视角。

**When to use:** 需要灵活组合的多部署系统。
**When NOT to use:** 单一部署或插件集固定的系统。

**Example in dsh:** `packages/bundle/base/cordis.patch.yml` + `packages/boot/app-boot/src/index.ts:mountRootInclude()`

### Pattern: Capability Seam Three-Role Split

**Context:** 有多个可交换实现的能力（local/remote/sandboxed executor）。

**Problem:** Provider 替换不应影响 model-facing 契约。

**Mechanism:** 拆分为 Service Definition（abstract class + vocabulary）/ Service Provider（implementation）/ Consumer（model-facing tool）。Consumer 通过 `inject` 声明依赖，不 import provider 类型。

**Consequence:** Provider 独立版本化。但增加包数量和 boilerplate。

**When to use:** 能力有 ≥2 个 conceivable provider。
**When NOT to use:** 只有一个实现的简单能力。

**Example in dsh:** bash trio: `dsh-shell` / `dsh-bash-local`+`dsh-bash-sandbox` / `dsh-tool-bash`

### Pattern: Scope-Filtered Event Dispatch

**Context:** 一个进程中运行多个 agent，每个 agent 有自己的事件监听器。

**Problem:** 全局事件监听器会收到所有 agent 的事件，无法隔离。

**Mechanism:** 事件 carrier 携带 scope key。监听器注册时携带 scope tag。事件沿 scope 链向上传播——父 scope 的监听器收到子 scope 的事件，但子 scope 不收到父 scope 的事件。

**Consequence:** 多 agent 隔离不需要多进程。但 scope 链管理增加复杂度。

**When to use:** 单进程多 agent 系统。
**When NOT to use:** 单 agent 或多进程隔离系统。

**Example in dsh:** `packages/core/scope/src/index.ts` — `createScope()` + `scopeTarget()`

### Pattern: Cooperative Tool Concurrency with Ordered Commit

**Context:** Agent 一步中的多个工具调用可能可以并行，但结果必须按模型顺序提交。

**Problem:** 并行执行提高吞吐，但模型期望结果按调用顺序返回。

**Mechanism:** `isConcurrencySafe(args)` 分类器决定 parallel vs exclusive。Parallel 调用在 bounded pool 中执行，结果按模型顺序通过 head-of-line cursor 提交。Exclusive 调用形成 barrier。

**Consequence:** 安全的并行 + 有序的结果。但分类器必须保守——任何异常回退 exclusive。

**When to use:** Agent 有并行工具调用需求。
**When NOT to use:** 所有工具都是 exclusive 或没有并行需求。

**Example in dsh:** `packages/core/agent-loop/src/tool-calls.ts` — `executeToolCalls()` + `runGroup()`

---

## 13. Things Not to Copy

### Problem: 89 个包的过度细粒度拆分

**Evidence:** `packages/` 下有 ~89 个 `ctx.<service>` 声明，分布在约 44 个顶级目录中。例如 `packages/shell/` 拆分为 `shell/`、`tool-bash/`、`bash-local/`、`bash-sandbox/`、`shell-env/`、`tool-bash-persistent/`、`pwsh-sandbox/` 等独立包。

**Why it matters:** 每个包需要独立的 `package.json`、`tsconfig`、README、invariant 文件、injection wiring。对于中等规模项目，这是显著的 boilerplate 成本。

**Impact:** 认知负担——开发者需要理解 89 个服务之间的依赖关系。构建时间——tsdown 需要处理大量小包。

**Alternative:** 对于中等规模，将紧密耦合的 capability seam 合并为一个包（如 `shell` + `tool-bash` + `bash-local`），只在真正需要第二个 provider 时才拆分。

### Problem: Vendored Cordis 的 18 处本地修改

**Evidence:** `vendor/README.md` 记录了 18 处本地修改，包括 Fiber lifecycle hardening（关闭 3 个 reentrant disposal gaps）、transactional Loader/Include reconciliation、HMR initial-scan suppression 等。

**Why it matters:** 这些修改意味着 dsh 的 Cordis 不是标准上游——升级需要手动 re-apply 18 处修改。fork 维护成本高。

**Impact:** 框架升级困难。社区贡献者需要理解 fork 差异。

**Alternative:** 如果不需要完全控制框架层，使用标准 npm 依赖。如果需要控制，考虑将 fork 的修改上游化。

### Problem: `!!js` YAML 表达式的安全面

**Evidence:** `vendor/loader/src/config/utils.ts:5` — `evaluate()` 使用 `new Function('ctx', 'expr', 'with(ctx){return eval(expr)}')`。虽然 `loadLayeredEnv()` 限制了 bootstrap-only 变量，但 `!!js` 表达式仍是代码注入入口。

**Why it matters:** 任何能修改 `cordis.patch.yml` 的人都能执行任意代码。

**Impact:** 在多用户或不可信配置场景下是安全风险。

**Alternative:** 使用受限的模板语言（如 `${env.VAR}` 或 JSONPath）而非 `eval`。或者明确文档化 `cordis.patch.yml` 是 trusted 配置。

### Problem: Proxy-based Context 的调试困难

**Evidence:** Cordis Context 是 `new Proxy<this>(this, ReflectService.handler)`。所有属性访问经过 `get` trap → `internal/get` waterfall → fiber 链查找。

**Why it matters:** Stack trace 中充满 `ReflectService.handler` 调用。服务解析的延迟在热路径上累积。`this.ctx` 的 traceable binding 增加额外间接层。

**Impact:** 调试困难。性能——每次 `ctx.serviceName` 访问都是一次 proxy 查找。

**Alternative:** 对于中等规模，使用简单的 Map-based service registry 而非 Proxy。牺牲一些 DX（`ctx.get('tools')` vs `ctx.tools`）换取可调试性。

### Problem: 测试覆盖的不均匀

**Evidence:** 692 个测试文件覆盖了核心包（agent-loop 有 22 个测试文件），但 UI 包（`packages/client/`）的测试更偏向 snapshot。一些关键路径如 HMR partial reload 的并发边界主要靠 e2e 测试覆盖。

**Why it matters:** 核心机制有良好测试，但集成路径的覆盖依赖 e2e。

**Impact:** E2E 测试慢且不稳定。

**Alternative:** 不适用——dsh 的测试策略对其规模是合理的。此发现仅标记覆盖分布。

---

## 14. Failure Modes

### Plugin Failure

| 失败模式 | 处理 | 证据 |
|---|---|---|
| Load failed | `Fiber._reload()` 抛出 → fiber 进入 FAILED 状态 | `vendor/cordis/src/fiber.ts:646-673` |
| Dependency missing | Fiber 保持 PENDING → `inject` 声明未满足 | `vendor/cordis/src/fiber.ts:611-623` |
| Initialization failed | `boot()` catch → `ctx.fiber.dispose()` → 抛出 labelled error | `packages/boot/app-boot/src/index.ts:786-801` |
| Runtime error | Plugin callback 抛出 → fiber FAILED → 依赖该服务的 fiber 重新 PENDING | Cordis `_refresh()` 机制 |
| Teardown failed | `Fiber._unload()` 容器化每个 disposer 的失败 | `vendor/cordis/src/fiber.ts:675-696` + 本地修改 #6 |

### Tool Failure

| 失败模式 | 处理 | 证据 |
|---|---|---|
| Timeout | `tools/execute` waterfall 的 wrapper 可注入超时 | `packages/core/tools/src/index.ts` |
| Exception | `dispatch()` 的 `.then()` / `.catch()` 捕获 → `schedulerFailure` | `packages/core/agent-loop/src/tool-calls.ts:178-181` |
| Malformed output | `createSuccessResult()` 校验 output schema | `packages/core/tools/src/index.ts` |
| Retry | LLM 层有 `llm-retry` 策略；tool 层无内置 retry | `packages/llm/llm-retry/src/index.ts` |
| Cancellation | Abort 信号传播 → 已启动调用 drain → 未启动调用获得 synthetic error result | `packages/core/agent-loop/src/tool-calls.ts:237-241` |

### Agent Failure

| 失败模式 | 处理 | 证据 |
|---|---|---|
| Model failure | `assembler.finish.kind === 'error'` → `agent/request-error` waterfall → 可 retry | `packages/core/agent-loop/src/agent.ts:354-371` |
| Context failure | Compaction 在 `context-overflow` 时触发 | `packages/compaction/compaction-basic/src/index.ts` |
| Tool loop failure | Driver boundary containment: `kick()` catch → driver 退出 → `whenIdle()` resolve | `packages/core/agent-loop/src/agent.ts:210-223` |
| Session interruption | Crash recovery: JSONL 撕裂尾部修复 / SQLite scanRows | `packages/session/session-persistence-jsonl/` |
| Process restart | Resume: `AgentRegistry.resume()` → 加载持久化日志 → 重建 session | `packages/core/agent/src/index.ts:424-430` |

### Configuration Failure

| 失败模式 | 处理 | 证据 |
|---|---|---|
| Invalid config | `resolveConfig()` → schema 验证 → `ValidationError` | `vendor/cordis/src/fiber.ts:50-62` |
| Missing config | Include 的 `initial` 回退（仅 `ENOENT`） | `vendor/include/src/index.ts:273-289` |
| Conflicting plugins | `ctx.provide()` 同 scope 重复注册 → 抛错 | `vendor/cordis/src/reflect.ts:277-305` |
| Incompatible versions | pnpm-workspace.yaml `peerDependencyRules` + `patchedDependencies` | `pnpm-workspace.yaml` |

---

## 15. Trade-offs

| Design | Gains | Costs | Worth it when | Avoid when |
|---|---|---|---|---|
| Plugin architecture | 可替换性、可组合性、可扩展性 | Indirection、调试困难、认知负担 | 大规模多部署系统 | 单一部署或小团队 |
| Event sourcing session | Replay/fork/audit 结构保证 | 派生成本随日志增长 | 需要 replay/fork 的系统 | 简单无状态 chatbot |
| Cordis DI (inject) | 声明式依赖、自动排序 | 学习曲线、PENDING 诊断 | 大量服务互相依赖 | 少量服务或手动排序可行 |
| Waterfall events | 声明式扩展、around-middleware | 线性执行、链长延迟 | 需要插件扩展 loop 步骤 | 性能关键路径 |
| Scope isolation | 单进程多 agent | scope 链管理复杂度 | 多 agent 同进程 | 单 agent |
| Patch-layer config | 可 diff 组合、声明式 | `!!js` eval 安全面 | 多部署/平台 | 单一配置 |
| Capability seam 3-role | Provider 独立版本化 | 包数量和 boilerplate | ≥2 个 conceivable provider | 单实现能力 |
| Vendored framework | 完全控制、可审计、可 patch | fork 维护、升级困难 | 框架层需要深度定制 | 使用标准依赖即可 |
| Proxy-based Context | DX（`ctx.tools` vs `ctx.get('tools')`） | 调试困难、性能开销 | DX 是优先级 | 可调试性优先 |
| DeepFreeze everything | 不可变性保证、防意外修改 | 序列化开销 | 跨边界数据安全 | 性能敏感路径 |

**dsh 在什么规模下值得采用：**

- **值得：** 需要 multi-deployment agent harness（web + headless + custom profiles）、需要 replay/fork、需要 plugin 生态、团队 ≥5 人
- **不值得：** 单一 CLI agent、不需要 replay、团队 1-2 人、快速原型

---

## 16. Comparison with Other Agent Harnesses

| Dimension | DeepSeek Harness | Claude Code |
|---|---|---|
| Extension model | Config-driven plugin composition (YAML patch layers) | MCP + hooks + plugin marketplace + skills |
| Agent loop | Explicit `while (await turn())` with turn/step boundary | Hierarchical agent with Task tool delegation |
| Tool model | `defineTool()` + 3-phase pipeline (prepare→dispatch→finalize) + scope-filtered | Tool execution engine with permission system |
| State model | Event-sourced session (append-only log, derived messages) | Session persistence in `~/.claude/sessions/` |
| Lifecycle | Cordis Fiber state machine (PENDING→LOADING→ACTIVE→UNLOADING→DISPOSED) | Not publicly documented in detail |
| Dependency model | `inject` declaration + service registry | Not publicly documented in detail |
| Configuration | Patch-layer YAML composition + `!!js` expressions | Hierarchical config (project + user + CLI) |
| Runtime composition | All capabilities are Cordis plugins in one process | Modular agentic framework |
| Sandbox | Multi-platform (bwrap/landlock/seatbelt/windows-acl) | Sandbox networking + file system guards |
| Subagent | Capability seam with multiple providers (spawn/fork/acp/codex/claude-code) | Task tool with subagent orchestration |
| Compaction | Event-sourced (surface replace) + tool-result pruner | Auto-compaction with hard failure at 32MB |
| Persistence | JSONL (Zstandard) / SQLite (FTS5), write-behind batching | Session files in `~/.claude/sessions/` |

**关键差异：**

1. **dsh 的 plugin 系统比 Claude Code 更彻底**——agent loop 自身是可替换插件，而 Claude Code 的 agent loop 是内置的
2. **dsh 的事件溯源比 Claude Code 的 session 持久化更严格**——log IS state，而非 log + mutable state
3. **dsh 的 sandbox 更深入**——多平台 native runner（bwrap/landlock/seatbelt/windows-acl），而 Claude Code 主要做网络和路径限制
4. **Claude Code 的 MCP 生态更成熟**——标准化协议、OAuth、marketplace；dsh 的 plugin 是 npm 包 + Cordis 配置
5. **dsh 的配置组合更灵活但更复杂**——patch-layer + `!!js` vs Claude Code 的层级配置

> **Note:** Claude Code 的对比基于公开文档和 CHANGELOG，不是源码级分析。标记为 **Strong inference**。

---

## 17. Open Questions

1. **Cordis fork 的长期维护策略**——18 处本地修改是否会尝试上游化？如果不上游化，如何跟踪上游安全修复？
2. **`!!js` 表达式的安全模型**——在多用户或不可信配置场景下，是否有计划替换为受限模板语言？
3. **HMR partial reload 的并发边界**——`analyzeChanges()` 的依赖图分类是否覆盖所有 edge case？测试主要靠 e2e。
4. **Long-term memory**——dsh 明确没有 long-term memory system（session log 是唯一持久化）。是否有计划添加跨 session 的知识库？
5. **89 个包的演化方向**——随着项目成熟，是否会合并部分包以降低 boilerplate？
6. **Python SDK 的定位**——`python/sdk-runtime/` 是独立部署还是 dsh 的子集？与 TypeScript 运行时的关系？
7. **0.1.0-rc.5 的稳定性**——README 明确说 "THERE WILL BE COMPATIBILITY-BREAKING CHANGES"。哪些 API 最可能变？

---

## 18. Final Takeaways

**如果你准备设计自己的 Agent Harness，读完这份报告，你具体学到了什么：**

1. **事件溯源 Session 是值得的**——把对话状态建模为只追加事件日志，消息历史从日志派生。这给你 replay/fork/audit/持久化的结构保证，而非 bolt-on。dsh 的实现（`Session` + `SurfaceManager` + `deriveMessages()`）是一个可参考的蓝图。

2. **Waterfall 是 agent loop 扩展的正确抽象**——不要让用户修改 loop 代码。把每一步暴露为 waterfall 事件（`pre-step`、`request`、`llm/stream`、`tools/pre-execute`、`tools/execute`、`tools/post-execute`），让 plugin 通过 `next()` 委托。这比 callback 或 middleware chain 更声明式。

3. **Capability Seam 三角色防止 provider churn**——当能力有多个实现时，拆分为 definition/provider/consumer 三个包。Consumer 通过 service key 发现 provider，不 import provider 类型。替换 provider 是配置变更，不触及 consumer。

4. **Patch-layer 配置组合解决部署多样性**——不要用编程式组合。用 YAML patch 层（base → mode → profile → user → overlay），让 `dump-config` 打印最终组合。这比环境变量或 feature flag 更可审计。

5. **Durable invariant 检查是运行时保证**——核心 invariant（如 "model-visible means logged"）应在运行时强制检查，而非仅靠代码审查。在关键操作前注册 `prepend: true` 监听器。

6. **不要照搬 89 个包的拆分**——对于中等规模，这是过度工程化。只在真正需要第二个 provider 时才拆分 capability seam。紧密耦合的包保持合并。

7. **不要照搬 vendored 框架**——除非你的框架层需要深度定制（dsh 有 18 处本地修改）。标准 npm 依赖的升级成本远低于 fork 维护成本。

8. **不要把 session/context 等同于 long-term memory**——dsh 明确没有 long-term memory。Session log 是会话级状态，不是跨会话知识库。如果你需要 memory，需要额外设计。

9. **Proxy-based Context 是 DX vs 可调试性的 trade-off**——`ctx.tools` 比 `ctx.get('tools')` 更优雅，但 Proxy 使 stack trace 难以阅读。对于中等规模，简单的 Map-based registry 可能更合适。

10. **"Everything is a plugin" 需要精确理解**——dsh 的所有业务能力是插件，但框架本身（Cordis）不是。如果你选择这个路线，明确区分框架层（不可替换）和业务层（全部插件化）。

---

## Appendix A — Important Files

| 文件 | 职责 |
|---|---|
| `apps/cli/src/bin.ts` | CLI 入口，mode 分发 |
| `apps/cli/src/profile-boot.ts` | Profile 组合 + boot() 调用 |
| `packages/boot/app-boot/src/index.ts` | `boot()` 函数 + `mountRootInclude()` |
| `packages/boot/app-boot/src/profile.ts` | Profile 模板 + 初始化 |
| `packages/bundle/base/cordis.patch.yml` | ~60 个插件的声明式组合 |
| `packages/core/agent-loop/src/agent.ts` | `ReactLoopAgent` — agent loop 实现 |
| `packages/core/agent-loop/src/tool-calls.ts` | Tool 调度器（并行/串行 + 有序提交） |
| `packages/core/agent-loop/src/invariant.ts` | LLM 请求 invariant 检查 |
| `packages/core/agent/src/index.ts` | `AgentRegistry` — agent 服务 + initiator scope |
| `packages/core/session/src/index.ts` | `Session` — 事件溯源会话 |
| `packages/core/session/src/surface.ts` | `SurfaceManager` — 消息历史派生 |
| `packages/core/scope/src/index.ts` | `createScope()` + `scopeTarget()` |
| `packages/core/tools/src/index.ts` | `ToolRuntime` — 工具注册表 + 执行管道 |
| `packages/core/tools/src/schema.ts` | `defineTool()` — 类型安全工具定义 |
| `packages/llm/llm/src/index.ts` | `LlmRuntime` — adapter 注册表 + 流式调用 |
| `packages/llm/llm/src/assembler.ts` | `BlockAssembler` — chunk→message 组装 |
| `packages/llm/llm/src/error.ts` | `LlmError` + 错误码体系 |
| `packages/sandbox/sandbox/src/index.ts` | `SandboxProvider` — 沙箱抽象 |
| `packages/sandbox/sandbox-local/src/index.ts` | 多平台沙箱后端（bwrap/landlock/seatbelt/windows-acl） |
| `packages/session/session-persistence/src/coordinator.ts` | `PersistenceCoordinator` — 后端无关写路径 |
| `packages/compaction/compaction/src/index.ts` | `CompactionEngine` — 压缩抽象 |
| `packages/compaction/compaction-basic/src/index.ts` | `BasicCompactionEngine` — 自动+手动压缩 |
| `packages/subagent/subagent/src/index.ts` | `SubagentRuntime` — 子代理 seam |
| `packages/skill/skill/src/index.ts` | `SkillRegistry` — skill seam |
| `packages/skill/tool-skill/src/index.ts` | `skill` 工具 — skill 目录 + 加载器 |
| `vendor/cordis/src/context.ts` | `Context` — Proxy-based 服务仓库 |
| `vendor/cordis/src/fiber.ts` | `Fiber` — 插件生命周期 |
| `vendor/cordis/src/registry.ts` | `RegistryService` — 插件注册 |
| `vendor/cordis/src/events.ts` | `EventsService` — 5 种 dispatch mode |
| `vendor/cordis/src/service.ts` | `Service` — 服务基类 |
| `vendor/cordis/src/reflect.ts` | `ReflectService` — Context proxy handler |
| `vendor/loader/src/index.ts` | `Loader` — 配置驱动加载 |
| `vendor/loader/src/config/entry.ts` | `Entry` — 插件 entry + diff/restart |
| `vendor/include/src/index.ts` | `Include` — YAML patch 系统 |
| `docs/architecture.md` | 架构文档 |
| `docs/cordis-primer.md` | Cordis 入门 |
| `AGENTS.md` | Agent 指令 |
| `vendor/README.md` | Vendored Cordis 修改日志 |

---

## Appendix B — Evidence Index

| Claim | Evidence | Confidence |
|---|---|---|
| Everything is a plugin (business capabilities) | `packages/bundle/base/cordis.patch.yml` — ~60 plugin rows including agent-loop itself | Confirmed |
| Agent loop is an explicit while loop | `packages/core/agent-loop/src/agent.ts:212` — `while (await this.turn()) {}` | Confirmed |
| Session is event-sourced with derived messages | `packages/core/session/src/index.ts` — `Session` class + `SurfaceManager` | Confirmed |
| Model-visible means logged (invariant) | `packages/core/agent-loop/src/invariant.ts` — `llm/stream` prepend listener | Confirmed |
| Cordis provides plugin lifecycle, DI, events, scope | `vendor/cordis/src/` — Context, Fiber, Registry, Events, Service | Confirmed |
| dsh built agent loop, tools, session, sandbox on top of Cordis | `packages/core/` — all import `@deepseek-ai/cordis` | Confirmed |
| Patch-layer composition: bundle → profile → home → overlay | `packages/boot/app-boot/src/index.ts:composeProfile()` + `profile-boot.ts:composeProfile()` | Confirmed |
| Capability seam three-role split is a deliberate design decision | `.agents/notes/implemented/architecture/2026-06-13-capability-seams.md` | Confirmed |
| Waterfall events are the primary extension mechanism | `docs/architecture.md` — "Events are the extension points" | Confirmed |
| Tool execution is 3-phase: prepare → dispatch → finalize | `packages/core/tools/src/index.ts` — `TOOL_RUNTIME_SCHEDULER` interface | Confirmed |
| Scope provides per-agent registration isolation | `packages/core/scope/src/index.ts` — `createScope()` + `scopeTarget()` | Confirmed |
| Persistence is plugin concern (not in Session) | `packages/core/session/src/index.ts` — Session has no persistence code; `packages/session/session-persistence/` | Confirmed |
| JSONL and SQLite backends with crash recovery | `packages/session/session-persistence-jsonl/src/index.ts` + `session-persistence-sqlite/src/index.ts` | Confirmed |
| Compaction uses surfaceOp:replace, not log mutation | `packages/compaction/compaction/src/types.ts` — `compaction/*` events are log-only | Confirmed |
| Sandbox is multi-platform with fail-closed design | `packages/sandbox/sandbox-local/src/index.ts` — PLATFORM_CHAINS + DENIAL_SIGNATURES | Confirmed |
| Subagent is a capability seam with multiple providers | `packages/subagent/subagent/src/index.ts` — `registerProvider()` + 6 provider packages | Confirmed |
| Skill system discovers from .agents/skills/ and .dsh/skills/ | `packages/skill/skill-filesystem/src/index.ts` — rank-based discovery | Confirmed |
| Vendored Cordis has 18 local modifications | `vendor/README.md` — "Local modifications" section | Confirmed |
| 89 ctx.<service> declarations | `grep -rn "interface Context {" packages/` — 89 matches | Confirmed |
| 692 test files + 129 e2e tests | `find . -name '*.spec.ts'` / `find . -name '*.e2e.ts'` | Confirmed |
| ~200K lines of TS in packages | `wc -l` on packages TS files (non-test) | Confirmed |
| Agent Notes are machine-checked decision records | `scripts/verify-agent-note-format.ts` + `verify-archived-agent-notes.ts` | Confirmed |
| Cordis adoption predates the current 50 commits | Git history limited by shallow clone; earliest visible commit references Cordis release | Strong inference |
| Claude Code comparison details | Public docs + CHANGELOG, not source-level analysis | Strong inference |
| `!!js` eval is the main config security surface | `vendor/loader/src/config/utils.ts:5` — `new Function(...)` | Confirmed |
| Proxy-based Context causes debugging difficulty | `vendor/cordis/src/context.ts:74` — `new Proxy<this>(this, ...)` | Confirmed |
| dsh has no long-term memory system | No memory/knowledge package in packages/; session log is sole persistence | Confirmed |
| HMR partialReload handles concurrent file changes | `vendor/hmr/src/index.ts:400-549` — `partialReload()` + `analyzeChanges()` | Confirmed |
| `dsh plugin` forwards to pnpm + reconciles bundle list | `apps/cli/src/plugin.ts` — `runPlugin()` + `reconcilePlugins()` | Confirmed |
| The project is pre-release with breaking changes expected | `README.md` — "THERE WILL BE COMPATIBILITY-BREAKING CHANGES" | Confirmed |
