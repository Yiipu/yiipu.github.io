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

HP::HP()
{
    this->len = 0;
    for (int i = 0; i < HP_MAX; i++)
    {
        this->s[i] = 0;
    }
}

HP::HP(int init)
{
    this->len = 0;
    for (int i = 0; i < HP_MAX; i++)
    {
        this->s[i] = 0;
    }
    while (init > 0)
    {
        this->s[this->len++] = init % HP_BASE;
        init /= HP_BASE;
    }
}

HP::HP(const char *init)
{
    this->len = 0;
    for (int i = 0; i < HP_MAX; i++)
    {
        this->s[i] = 0;
    }
    while (init[this->len] != '\0' && this->len < HP_MAX)
    {
        this->s[this->len] = init[this->len] - '0';
        this->len++;
    }
}

// 用 10 进制整数初始化
HP HP::operator=(int init)
{
    this->len = 0;
    for (int i = 0; i < HP_MAX; i++)
    {
        this->s[i] = 0;
    }
    while (init > 0)
    {
        this->s[this->len++] = init % HP_BASE;
        init /= HP_BASE;
    }
    return *this;
}
// 用 10000 进制的字符串初始化
HP HP::operator=(const char *init)
{
    this->len = 0;
    for (int i = 0; i < HP_MAX; i++)
    {
        this->s[i] = 0;
    }
    while (init[this->len] != '\0' && this->len < HP_MAX)
    {
        this->s[this->len] = init[this->len] - '0';
        this->len++;
    }
    return *this;
}

bool HP::operator==(const HP &b)
{
    if (this->len != b.len)
        return false;
    for (int i = 0; i < this->len; i++)
    {
        if (this->s[i] != b.s[i])
            return false;
    }
    return true;
}

bool HP::operator!=(const HP &b)
{
    return !(*this == b);
}

bool HP::operator>(const HP &b)
{
    if (this->len > b.len)
        return true;
    if (this->len < b.len)
        return false;
    for (int i = this->len - 1; i >= 0; i--)
    {
        if (this->s[i] > b.s[i])
            return true;
        if (this->s[i] < b.s[i])
            return false;
    }
    return false;
}

bool HP::operator<=(const HP &b)
{
    return !(*this > b);
}

bool HP::operator<(const HP &b)
{
    return !(*this > b || *this == b);
}

bool HP::operator>=(const HP &b)
{
    return !(*this < b);
}

HP HP::operator+(const HP &b)
{
    HP c;
    c.len = this->len > b.len ? this->len : b.len;
    for (int i = 0; i < c.len; i++)
    {
        if (this->s[i] + b.s[i] + c.s[i] > HP_BASE)
        {
            c.s[i + 1]++;
            c.s[i] += this->s[i] + b.s[i] - HP_BASE;
        }
        else
        {
            c.s[i] += this->s[i] + b.s[i];
        }
    }
    if (c.s[c.len] > 0)
        c.len++;
    return c;
}

HP HP::operator-(const HP &b)
{
    if (*this < b)
        abort();

    HP c;
    c.len = this->len;
    for (int i = 0; i < c.len; i++)
    {
        c.s[i] += this->s[i] - b.s[i];
        if (c.s[i] < 0)
        {
            c.s[i + 1] -= 1;
            c.s[i] += HP_BASE;
        }
    }
    while (c.s[c.len - 1] == 0)
    {
        c.len--;
    }
    return c;
}

HP HP::operator*(const HP &b)
{
    HP c;
    c.len = this->len + b.len;
    for (int i = 0; i < this->len; i++)
    {
        for (int j = 0; j < b.len; j++)
        {
            int index = i + j;
            c.s[index] += this->s[i] * b.s[j];
            if (c.s[index] > HP_BASE)
            {
                c.s[index + 1] += 1;
                c.s[index] -= HP_BASE;
            }
        }
    }
    if (c.s[c.len - 1] == 0)
        c.len--;
    return c;
}

HP HP::operator+=(const HP &b)
{
    *this = *this + b;
    return *this;
}

HP HP::operator-=(const HP &b)
{
    *this = *this - b;
    return *this;
}

HP HP::operator*=(const HP &b)
{
    return *this * b;
}

// 以 10 进制输出
std::ostream &operator<<(std::ostream &os, HP &hp)
{
    os << hp.s[hp.len - 1];
    for (int i = hp.len - 2; i >= 0; i--)
    {
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
