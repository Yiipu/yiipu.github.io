---
title: 用 C++ 实现一个高精度整数类
author: yiipu
date: 2024-03-14 20:00:00 +0800
categories: [算法, 数据结构]
tags: [cpp, 重载]
math: true
description: 本文介绍高精度**正整数**的 C++ 类的实现，利用了 C++ 的符号重载特性，使用倒序压位存储的数组作为数据结构。
---

固定字节的 int 类型都存在一个有限的表示范围，而高精度算法可以满足大数字或高精度的计算需求。

本文介绍高精度**正整数**的 C++ 类的实现，利用了 C++ 的符号重载特性，使用倒序压位存储的数组作为数据结构。

## 符号重载

借助 C++ 的符号重载特性，我们在写高精度数的计算式时，可以直接使用 `+-*/` 等符号，而不必显式地调用函数。

### 语法

重载的运算符是带有特殊名称的函数，函数名是由关键字 operator 和其后要重载的运算符符号构成的。与其他函数一样，重载运算符有一个返回类型和一个参数列表。[^overload]

```text
[返回类型] operator[符号]([参数列表])
```

### 用例

#### 重载加法运算符

```cpp
HP HP::operator+(const HP &b);
```

#### 重载输出流运算符

1. 作为类的成员函数
    ```cpp
    class HP{
        std::ostream &operator<<(std::ostream &os);
    }
    ```
2. 作为类的友函数 [^output]
    ```cpp
    class HP{
        friend std::ostream &operator<<(std::ostream &os, const HP &hp);
    }
    ```

这两种方式的区别在于，如果将 `<<` 重载为类的成员函数，那么其左操作数必须是类的对象。于是，在使用 `<<` 时的语句就是 `hp << std::cout`，而不是 `std::cout << hp`。

为了能使用 `std::cout << hp`，我们要将 `<<` 重载为友函数：既能访问类的私有成员，又是全局函数。

值得注意的是，由于重载的运算符将返回原始 ostream 对象的引用(`return os;`)，我们才可以合并插入，例如 `std::cout << hp << std::endl`。

## 倒序压位存储

在一个长度为 n 的数组里存储正整数，很直观地，我们可以像在格纸上书写十进制数一样，将每一位数字存为一个数组元素，于是这个数组可以表示 $0$ ~ $10^n - 1$ 范围内的整数，总共 $10^n$ 个。

但是，如果用一个 unsigned int（4字节） 数组，每个数组元素可以保存 $0$ ~ $2^{32} - 1$ 范围内的整数，这样做就显得太浪费了。

那么，我们只要提高进制不就可以了吗？实际上我们还要考虑两个需求：

1. 数组元素在做乘法时不能造成整数溢出。
2. 方便打印。

为了满足 `1.` ，数组元素不能超过 $\sqrt{2^{32} - 1} \approx 65535$。
为了满足 `2.` ，提高的进制应该是 10 的正整数次幂。

综合以上两点，提高的进制应该是 10000 进制，也就是每 4 位数字压入一个元素。

最后，为了方便进位，低位在数组的低地址处，与我们的书写习惯相反，也就是倒序。

## 部分实现

### 类声明

```cpp
#pragma once
#include <iostream>

#ifndef HP_MAX
#define HP_MAX 1001
#endif

#define HP_BASE 10000

// 高精度正整数
class HP
{
    int len;                // 位数
    unsigned int s[HP_MAX]; // 低位在低地址
public:
    HP();
    HP(int init);
    HP(const char *init);

    HP operator=(int init);
    HP operator=(const char *init);

    bool operator==(const HP &b);
    bool operator!=(const HP &b);
    bool operator>(const HP &b);
    bool operator<=(const HP &b);
    bool operator<(const HP &b);
    bool operator>=(const HP &b);

    HP operator+(const HP &b);
    HP operator-(const HP &b);
    HP operator*(const HP &b);

    HP operator+=(const HP &b);
    HP operator-=(const HP &b);
    HP operator*=(const HP &b);

    friend std::ostream &operator<<(std::ostream &os, HP &hp);
    friend std::istream &operator>>(std::istream &is, HP &hp);
};
```

### 初始化

```cpp
// 用 10 进制整数初始化
HP HP::operator=(int init)
{
    this->len = 0;
    for (int i = 0; i < HP_MAX; i++) {
        this->s[i] = 0;
    }
    while (init > 0) {
        this->s[this->len++] = init % HP_BASE;
        init /= HP_BASE;
    }
    return *this;
}
```

### 输入输出流

```cpp
std::ostream &operator<<(std::ostream &os, HP &hp)
{
    os << hp.s[hp.len - 1];
    for (int i = hp.len - 2; i >= 0; i--) {
        os.width(4);
        os.fill('0');
        os << hp.s[i];
    }
    return os;
}

std::istream &operator>>(std::istream &is, HP &hp)
{
    char s[HP_MAX];
    is >> s;
    hp = s;
    return is;
}
```

## 源码

<a href="/assets/raw/hp.h" download><i class="fa fa-download"></i> Download</a>

---

[^overload]: C++ 中的运算符重载，[菜鸟教程](https://www.runoob.com/cplusplus/cpp-overloading.html)
[^output]: 为自己的类重载 << 运算符，[Microsoft Learn](https://learn.microsoft.com/zh-cn/cpp/standard-library/overloading-the-output-operator-for-your-own-classes?view=msvc-170)