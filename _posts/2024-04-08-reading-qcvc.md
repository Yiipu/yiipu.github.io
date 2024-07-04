---
title: 量子计算科普文章随记
categories: [笔记]
tags: [量子]
description: 读 [Quantum computing for the very curious](https://quantum.country/qcvc) 随记.
math: true
---

阅读原文: [Quantum computing for the very curious](https://quantum.country/qcvc)
by [Andy Matuschak](https://andymatuschak.org/) and [Michael Nielsen](https://michaelnielsen.org/)

## Introduction

If humanity ever makes contact with alien intelligences, will those aliens possess computers? In science fiction, alien computers are commonplace. If that's correct, it means there is some way aliens can discover computers independently of humans. After all, we’d be very surprised if aliens had independently invented Coca-Cola or Pokémon or the Harry Potter books. If aliens have computers, it’s because computers are the answer to a question that naturally occurs to both human and alien civilizations.

> 非常有趣!

Is there a (single) universal computing device which can efficiently simulate any other physical system?

> 计算机系统使用某种物理系统(如电子和磁场)来模拟逻辑系统, 进而可以模拟任何其他的物理系统.

## Part I: The state of a qubit

### qubit

- qubits have a state
- much like a bit, that state is an abstract mathematical object
- but whereas a bit’s abstract state is a number, 0 or 1, the state of a qubit is a2-dimensional vector
- we call the 2-dimensional vector space where states live state space

### A medium which makes memory a choice

> 作者之一 [Andy Matuschak](https://andymatuschak.org/) 正致力于创造一种名为 `mnemonic medium` 的媒介, 以帮助人们理解,记忆以及使用所看到的内容. 其显著特点是整合间隔重复测试(spaced-repetition testing). 登记了邮箱的读者还会按一定时间规律收到复习提醒. 这个时间规律是: 5天-2周-1个月-2个月.

For the most part our memories work in a haphazard manner. We read or hear something interesting, and hope we remember it in future. Spaced-repetition testing makes memory into a choice.

> 基于 spaced-repetition 的软件有很多, 比如不背单词/space. 除了单词以外, 当你想自己写一些记忆卡片时, 可以试试 Obsidian 的 [Spaced Repetition](obsidian://show-plugin?id=obsidian-spaced-repetition) 插件.

### Connecting qubits to bits: the computational basis states

```math
\left | 0 \right \rangle := \begin{bmatrix}1\\0\end{bmatrix}
```
```math
\left | 1 \right \rangle := \begin{bmatrix}0\\1\end{bmatrix}
```

### How to use (or not use!) the questions

Mathematics is a process of staring hard enough with enough perseverance at the fog of muddle and confusion to eventually break through to improved clarity. (William Thurston,
Fields-medal winning mathematician)

### How to approach this essay?

略

### General states of a qubit

the quantum state of a qubit is a vector of unit length in a two-dimensional complex vector space known as state space.

### What does the quantum state mean? Why is it a vector in a complex vector space?

> 诺贝尔物理学奖得主 Steven Weinberg 认为，迄今为止关于量子力学还没有一个完美的解释，虽然有物理学家满足于自己的理论，但他们却满足于**不同**的理论。
> 传统的 bit 位只有两种状态，而且是确定的。那么量子位呢？量子位的状态是一个向量，还是概率？在尝试下定义之前，作者建议，还是先上手把玩一下吧。

So the strategy we’re taking is to start with the mathematics of quantum computing – we’ll keep getting familiar with qubits and the quantum state, and developing the consequences. Doing that is how we’ll build up intuition, and will give us the chops needed to come back and think harder about the meaning of the quantum state.

## Part II: Introducing quantum logic gates

> 量子逻辑门是量子计算潜能的来源，它为计算提供了更多的可能性

> 在介绍了 H 门后，作者提出一个问题，两个 H 门相连，会发生什么。在用定义计算之前，作者建议先猜一下，这可能有助于直觉的形成。

Before we compute, it’s worth pausing for a second to try guessing the result. The point of guessing isn’t to get it right – rather, it’s to challenge yourself to start coming up with heuristic mental models for thinking about what’s going on in quantum circuits.

