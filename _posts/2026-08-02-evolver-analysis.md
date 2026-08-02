---
title: EvoMap/evolver 仓库分析报告
date: 2026-08-02 11:00:00 +0800
categories:
  - AI
  - Agent
tags:
  - Evolver
  - 分析报告
description: 对 EvoMap/evolver - 面向 AI agent 的进化策略框架的开源仓库分析报告
---
## 一句话摘要

Evolver 是一个基于 GEP（基因组进化协议）的 AI Agent 自进化引擎，将 ad hoc 的 prompt 调整转化为可审计、可复用的结构化进化资产（Genes、Capsules、Events），核心引擎以混淆形式分发，正从 GPL-3.0 转向 source-available 许可证。

---

## 值得关注的理由

1. **学术背书的协议设计**。arXiv 论文（2604.15097）通过 4,590 次对照实验证明，紧凑的 Gene 表示比文档式 Skill 包提供更密集、更稳健的控制信号。在 CritPt 基准上，Gene 进化将配对基础模型从 9.1% 提升到 18.57%、从 17.7% 提升到 27.14%。在 agent 自进化领域，这是少见的"有论文撑腰"的设计。

2. **"Prompt 生成器而非代码修补器"的克制定位**。Evolver 明确不自动编辑源代码。它扫描日志、选择 Gene、输出协议约束的 GEP prompt 到 stdout，由宿主运行时决定是否执行。大多数竞品倾向于自主修改代码，这种"建议但不执行"的设计反直觉但安全。

3. **正在发生的知识产权博弈**。README 公开指控另一个项目在无署名的情况下发布了"惊人相似"的记忆/技能/进化资产设计，并宣布从 GPL-3.0 转向 source-available。核心引擎已以混淆形式分发。这是开源 AI 工具生态中 IP 保护的实时案例。

---

## 项目概况

| 字段 | 值 |
|------|-----|
| Star 数 | 8,914 |
| Fork 数 | 825 |
| 年龄 | ~6 个月（2026-02-01 创建） |
| 最后推送 | 2026-07-27 |
| 主要编程语言 | JavaScript |
| 许可证 | GPL-3.0-or-later（核心模块混淆，转向 source-available） |
| 最新稳定版 | v1.93.0（2026-07-23） |
| 最新预发布 | v2.0.0-beta.18（2026-07-27，同日发布 4 个 beta） |
| 阶段 | 成熟（接近主流） |
| 已归档 | 否 |
| Open Issues | 11 |

```text
JavaScript  ████████████████████████████████ 100%
```

单语言仓库，4.5MB JavaScript 代码，无次要语言。核心引擎模块以混淆形式分发。

---

## 作者视角

Evolver 由 EvoMap 团队构建，背靠 evomap.ai 网络。根据官方博客，OpenClaw（Evolver 的原生宿主运行时）被 OpenAI 收购后，开发者"一夜之间"构建了 EvoMap。

他们看到的核心问题：AI Agent 的 prompt 调整是 ad hoc 的。每次修改没有结构化记录、无法复用、无法审计。他们将生物进化概念映射到 agent 领域：

- **Gene** = 策略或模式的编码，不是自然语言 prompt，是结构化 JSON（id、signals_match、preconditions、strategy、constraints、validation、epigenetic_marks）
- **Capsule** = 验证过的成功解决方案，从真实执行轨迹中提取
- **Event** = 每次进化的审计记录

arXiv 论文的结论驱动了设计：文档式 Skill 包提供"不稳定、稀疏的控制信号"，而紧凑的 Gene 表示更稳健、更适合经验积累。这不是直觉判断，是实验结论。

v1.37.0 博客透露了进化方向：从"单循环修复"（只修已知问题）向"双循环进化"（从所有经验包括失败中主动学习）转移。`autoDistillFromFailures` 从失败 Capsule 中合成防御性 repair Gene，策略步骤以 GUARD/VERIFY/ROLLBACK 前缀标注。

作者明确选择不做 RL/LoRA 权重更新。MetaClaw（UNC/CMU/UCSC/Berkeley 的学术对标）走了 Cloud LoRA + RL-PRM 路线，Evolver 认为生态不成熟（Tinker/MindLab 非通用 API），且端到端结果只在 Kimi-K2.5 上验证过。这个取舍定义了 Evolver 的能力边界：prompt/策略层面的进化，不触及模型权重。换来的收益是完全离线、零 GPU 成本。

作者对知识产权保护极度敏感。README 中有一整段指控另一个项目（暗示是 Hermes Agent）在 2026 年 3 月发布了"惊人相似"的设计且未署名，链接到详细对比分析。这直接导致了 source-available 转型和核心引擎混淆分发。

---

## 核心架构

### GEP 进化循环（5 阶段）

```text
信号提取 → 资产选择 → Prompt 生成 → 外部执行 → 验证固化
    │           │           │              │           │
    │           │           │              │           ├─ 成功: 保存 Capsule + Event
    │           │           │              │           └─ 失败: git reset --hard 回滚
    │           │           │              └─ 宿主运行时 (OpenClaw/Cursor/Claude Code)
    │           │           └─ buildGepPrompt() 组装结构化 JSON
    │           └─ selector.js 按 Signal 评分匹配 Gene/Capsule
    └─ signals.js 扫描 memory/ 目录提取错误/性能/机会信号
```

Evolver 自身不执行进化。它生成 GEP prompt 输出到 stdout，由宿主运行时解释 `sessions_spawn(...)` 指令来触发后续动作。独立运行时这些指令只是文本。

### 三种突变策略

| 策略 | Innovate | Optimize | Repair | 适用场景 |
|------|----------|----------|--------|----------|
| balanced（默认） | 50% | 30% | 20% | 日常运行 |
| innovate | 80% | 15% | 5% | 系统稳定时快速迭代 |
| harden | 20% | 40% | 40% | 大改后稳定化 |
| repair-only | 0% | 20% | 80% | 紧急修复 |

### 记忆图与因果学习

Memory Graph 是持久化的因果学习系统，记录完整的因果链：SignalSnapshot → Hypothesis → Attempt → Outcome。`getAdvice()` 使用 Laplace smoothing + time decay 计算 (signal, gene) 组合的成功概率，自动屏蔽低效路径、优先有效路径。跨会话积累，越用越准。

`memoryGraphAdapter.js` 封装 `memoryGraph.js` 的核心逻辑，提供 `getAdvice`、`recordSignalSnapshot`、`recordHypothesis`、`recordAttempt`、`recordOutcome` 等接口。

**Epigenetic Marks** 是 Memory Graph 内的学习机制，用于环境特定适配。Gene 结构中包含 `epigenetic_marks` 字段。DeepWiki 分析显示其具体实现细节在混淆代码中无法确认。

### 失败学习（v1.37.0 新增）

`autoDistillFromFailures` 是完整的失败学习管道：

1. `collectFailureDistillationData()` — 从 `failed_capsules.json` 收集失败记录，按 gene + 失败原因分组
2. `analyzeFailurePatterns()` — 识别高频失败模式和重复约束违规
3. `synthesizeRepairGeneFromFailures()` — 从失败模式合成防御性 repair Gene，策略步骤以 GUARD/VERIFY/ROLLBACK 前缀标注
4. `autoDistillFromFailures()` — 整合上述步骤，默认阈值 5 个失败 Capsule 触发

合成的 Gene 与成功蒸馏的 Gene 走相同的 15+ 项硬验证管道。

### Skill 蒸馏（双向）

- **正向**：`skillDistiller.js` 从 Capsule 流生成 Gene
- **逆向**：`skill2gep.js` 将本地 Skill（Cursor、Claude Code、Codex）及其执行轨迹转换为 Gene + Capsule

Capsule 只从真实执行轨迹中产生，其 `execution_trace` 必须覆盖 `Gene.validation` 中的每一项。

### 安全架构（7 层漏斗）

1. **突变分类限制** — repair/optimize/innovate 三级风险控制
2. **爆炸半径限制** — 文件数量和路径上限，核心引擎代码受保护
3. **进程锁** — 禁止子进化进程生成，防 fork bomb
4. **稳定性偏好** — 高错误率时强制降级为 repair 模式
5. **验证命令白名单** — 只允许 `node`/`npm`/`npx` 前缀；禁止 `$(...)` / 反引号 / shell 操作符（`;` `&` `|` `>` `<`）；180 秒超时；cwd 锁定仓库根目录
6. **Git 回滚** — 验证失败触发 `git reset --hard`；审查模式 reject 执行 `git checkout -- .` + `git clean -fd`
7. **外部资产隔离** — A2A 获取的 Gene/Capsule 进入候选区，需 `--validated` 标志才能提升；验证命令审计同一安全检查

### 部署模式

| 模式 | 行为 |
|------|------|
| CLI（`evolver`） | 生成 prompt，输出 stdout，退出 |
| 守护进程（`evolver --loop`） | 后台循环，自适应休眠。用于验证者任务、worker 任务、维护 |
| 嵌入宿主（OpenClaw/Cursor/Claude Code/Codex/Kiro/opencode） | 宿主解释 stdout 指令触发后续动作 |

`--loop` 模式不是实时 agent 助手。它的 stdout 被 evolver 自身消费，不会被宿主运行时拾取。要让 evolver 观察和建议活跃的 agent 会话，需在会话内部调用 `evolver`。

### A2A 网络与验证者节点

连接 EvoMap Hub 后解锁：心跳（6 分钟）、Skill Store、Worker Pool、Evolution Circle、资产发布。每个 evolver 实例默认充当去中心化验证者：定期拉取 Hub 分配的验证任务，在沙箱中执行提议者声明的验证命令，提交 `ValidationReport`，赚取信用和声誉。

---

## 核心价值

| 价值点 | 新颖性 | 可用性 | 可移植性 | 说明 |
|--------|--------|--------|----------|------|
| GEP 协议（Gene/Capsule/Event 资产模型） | 5 | 3 | 4 | 用生物进化隐喻将 prompt 调整结构化为可审计资产，有论文背书 |
| "Prompt 生成器"克制定位 | 4 | 4 | 5 | 不自动改代码，只生成协议约束的 prompt，安全性高 |
| Memory Graph 因果学习 | 4 | 3 | 3 | signal→action→outcome 链条，Laplace smoothing + time decay 计算 (signal, gene) 成功概率 |
| 失败蒸馏（autoDistillFromFailures） | 4 | 3 | 3 | 从失败 Capsule 合成防御性 repair Gene，GUARD/VERIFY/ROLLBACK 前缀 |
| 多平台 Hook 集成 | 3 | 4 | 4 | Cursor/Claude Code/Codex/Kiro/opencode/OpenClaw 一键接入 |

GEP 协议是最值得深入理解的部分。论文数据显示，Gene 表示优于 Skill 文档，因为它提供了"更密集、更结构化的控制信号"。Skill 文档是自然语言，模型对其理解存在方差；Gene 是结构化 JSON，模型行为更可预测。每个 Gene 字段在定义时就可验证——能改多少文件、不能碰哪些路径、如何验证成功——不依赖 LLM 运行时判断。

这个发现对任何做 agent prompt 管理的团队都有参考价值。你的 prompt 管理策略可能需要从"写更好的文档"转向"编码更结构化的策略"。

---

## 竞争格局

| 项目 | 定位 | 与 Evolver 的关系 |
|------|------|-------------------|
| MetaClaw (arXiv 2603.17187) | 学术界 agent 自进化 | UNC/CMU/UCSC/Berkeley 联合发表，走"能力获取"路线（failure distillation、idle scheduling、semantic retrieval），有 MetaClaw-Bench（934 题）。Evolver 走"工程治理"路线（结构化 Gene、causal Memory Graph、7 层安全漏斗）。两者是同一问题的不同角度，不在同一 benchmark 上，不应直接对比 |
| Hermes Agent Skills | Agent 记忆 + 技能系统 | 被 Evolver 指控设计相似 |
| Cursor Rules | IDE 内 prompt 规则 | Evolver 的 `setup-hooks --platform=cursor` 是其超集 |
| OpenClaw | Agent 运行时 | Evolver 的原生宿主。OpenClaw 被 OpenAI 收购后，开发者构建了 EvoMap |

与 MetaClaw 的分野值得深入理解。MetaClaw 有权重更新循环（Cloud LoRA + RL-PRM），在 idle 时间窗口微调模型权重，理论上天花板更高。Evolver 明确放弃这条路，理由是 Cloud LoRA 生态不成熟且只在 Kimi-K2.5 上验证过。这个取舍意味着 Evolver 的进化被限制在 prompt/策略层面，无法触及模型权重——但换来了完全离线、零 GPU 成本的运行模式。

MetaClaw 的另一个优势是语义技能检索（embedding cosine similarity），处理"同义但不同表述"的匹配场景。Evolver 的 Gene 选择基于 signal 匹配，在这个维度上可能不如 embedding 方式灵活。

MetaClaw 论文自身承认的短板：无 rollback/version control、无安全/对抗评估、技能是自然语言无结构约束、无跨会话因果记忆、单机框架无跨节点经验复用。这些恰好是 Evolver 的强项。

真正的竞品是各 agent 框架内置的 prompt 管理能力。Evolver 不与具体 IDE 竞争，而是在"agent 自进化"这个细分赛道上定义品类。

---

## 生态定位与套利机会

**信息差**：Evolver 的 arXiv 论文和 MetaClaw 对比博客尚未被广泛讨论。8.9K star 与论文质量的比值暗示技术深度被低估。社区注意力集中在许可证争议上，而非技术贡献。论文中"Gene 优于 Skill 文档"的结论对整个 agent prompt 管理领域有方法论价值。

**技术借鉴**：

- GEP 的"信号→资产匹配→协议 prompt→外部执行→验证固化"循环可迁移到任何需要结构化决策的 agent 系统
- "生成建议但不执行"的安全模型适合需要 human-in-the-loop 的 agent 场景
- `autoDistillFromFailures` 的失败学习模式（从失败中合成防御性策略）可迁移到任何需要从错误中学习的系统
- Epigenetic Marks 的环境适配概念可迁移到多环境部署的 agent 配置管理

**生态位**：Evolver 占据"协议约束 + 可审计 + 网络化 + 离线"四轴交叉点。大多数 agent 框架有记忆但没有进化协议；有进化的系统没有审计链；有审计的系统不联网。EvoMap Hub 的 A2A 协议试图建立 agent 间的资产交换标准。

**趋势判断**：随着 agent 在生产环境部署增多，prompt 治理和进化审计的需求会上升。合规驱动的场景（金融、医疗）需要可追溯的 agent 行为变更记录——GEP 的 Event 审计链天然适配这个需求。Apple Silicon 本地 LLM 普及正在降低 Evolver 这类"离线 + 零 GPU"方案的部署门槛。

---

## 风险与不足

**核心风险：source-available 转型与混淆代码**

README 明确宣布从 GPL-3.0 转向 source-available，核心引擎已混淆。这意味着：

- 社区贡献者无法审查核心逻辑（DeepWiki 分析多次出现"内容被混淆，无法确定功能"）
- 安全审计依赖厂商自述，无法独立验证。安全模型描述详尽（命令白名单、shell 操作符过滤、180 秒超时），但这些逻辑在混淆的 `solidify.js` 中执行。用户无法独立验证安全检查是否如文档所述实现。对于一个声称"安全优先"的自进化系统，这是根本性矛盾
- GPL-3.0 历史版本仍可用，但后续更新可能改变条款

**公交车因子低**

| 贡献者 | 提交数 |
|--------|--------|
| 匿名（可能是核心开发者或 CI） | 107 |
| autogame-17 | 27 |
| 其他 7 人 | 各 1 |

两个核心贡献者，其余都是一次性贡献。autogame-17 可能是项目主力。

**无标准化公开 benchmark**

MetaClaw 有 MetaClaw-Bench（934 题 / 44 天模拟）。Evolver 只有单元和集成测试，`test/bench.test.js` 是第一步。论文中的 CritPt 实验是特定场景验证，不是通用可比较的基准。这使得外部评估者难以独立衡量 Evolver 的进化效果。

**提交活动趋势**

90 天 91 次提交（约每天 1 次）是健康信号。但 30 天仅 16 次显示近期放缓。提交类型分布（最近 100 条）：docs 13、fix 7、feat 3、refactor 2——文档占比 65%，可能意味着核心功能已稳定，也可能意味着活跃开发放缓。25 条使用 conventional commit 前缀（25%），对于 6 个月的项目提交规范尚可。

**Hub 网络的可持续性**

核心功能离线可用是优点，但 Hub 功能（Skill Store、Worker Pool、验证者网络）的可持续性取决于 EvoMap 团队运营。没有去中心化治理机制的迹象。验证者默认开启——用户可能不知道自己的 evolver 实例在为网络执行验证任务。

**11 个 open issues**

对于 8.9K star 的项目，11 个 open issue 不算积压。但无法获取 closed issue 数量来评估处理速度。

---

## 行动建议

**如果你要用它**

```bash
npm install -g @evomap/evolver
cd your-git-project
evolver           # 单次运行
evolver --review  # 人工审查模式（推荐首次使用）
evolver --loop    # 守护进程模式
```

适合：需要在 git 仓库中维护 agent prompt 且要求审计追踪的团队。不适合：需要 agent 自主修改代码的场景（Evolver 不做这个）；非 git 环境（Evolver 强制要求 git）。

注意：核心引擎混淆，安全声明无法独立验证。生产环境务必使用 `--review` 模式。验证者角色默认开启，如不想参与网络验证需设 `EVOLVER_VALIDATOR_ENABLED=0`。

**如果你要学它**

必读资源（按优先级）：

1. arXiv 论文 2604.15097 — Gene vs Skill 的实验论证，4,590 次对照实验
2. EvoMap 博客"From MetaClaw to Evolver" — 与学术对标的深度对比，理解设计取舍
3. README 的"Security Model"章节 — 7 层安全漏斗的参考设计
4. DeepWiki 的"GEP Protocol Components"和"Memory Graph & Causal Learning"章节 — 协议组件交互
5. `assets/gep/genes.json` — Gene 结构的实例

**如果你要 Fork 它**

已发布的 GPL-3.0 版本仍可自由使用。但 Fork 后面临两个问题：核心引擎混淆，无法修改核心逻辑；后续版本可能不再是 GPL。最值得优先改进的方向：

1. 用可读实现替换混淆模块，使安全声明可独立审计
2. 实现标准化公开 benchmark（类似 MetaClaw-Bench），使进化效果可外部验证
3. 增加 embedding 语义检索，弥补 signal 匹配在"同义不同表述"场景下的不足

**如果你要避免它**

以下情况考虑替代方案：需要完全开源可审计的代码（核心引擎混淆）；需要 agent 自主修改代码（Evolver 只生成 prompt）；需要模型权重层面的进化（Evolver 不做 RL/LoRA，看 MetaClaw 方向）；对许可证变更敏感（source-available 转型中）；需要在非 git 环境运行。

---

## 数据来源

| 来源 | 类型 | 用途 |
|------|------|------|
| GitHub API (`gh repo view`) | 原始数据 | 元数据：star、fork、创建时间、语言、许可证、release |
| GitHub API (`gh api`) | 原始数据 | 语言字节、贡献者、提交类型、提交计数、release 历史、README |
| DeepWiki MCP | AI 生成文档 | 架构深度分析：GEP 协议、安全架构、Memory Graph、A2A 协议、Epigenetic Marks、Skill 蒸馏 |
| README.md | 官方文档 | 功能描述、安全模型、使用方式、许可证声明、争议说明、平台集成 |
| arXiv:2604.15097 | 学术论文 | Gene vs Skill 实验论证，4,590 次对照实验 |
| EvoMap 官方博客 (v1.37.0) | 官方文章 | MetaClaw 对比、双循环进化、autoDistillFromFailures、设计取舍、Memory Graph 算法细节 |
| AgentConn | 第三方评测 | 上线首日数据（866 star）、GEP 技术描述（Karva notation）、使用场景分析 |
