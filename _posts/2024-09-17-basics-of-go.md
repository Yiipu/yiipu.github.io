---
title: Go 语言初见
categories:
  - 笔记
  - 语言
tags:
  - go
description: A Tour of Go 的学习笔记。部分练习题上传到了 gist
math: 
mermaid: 
image:
---
[A Tour of Go](https://go.dev/tour/welcome/2) 的学习笔记。部分练习题上传到了 [gist](https://gist.github.com/Yiipu/9cd19f251152c2649ab9f1604f2e52e3)。

参考 [Golang 入门 : Go语言的设计哲学](https://www.cnblogs.com/niuben/p/16172538.html)，Go 语言的设计哲学可以概括为：
- **简单**：关键字少，可读性好，内置功能完善（GC 边界检查 并发 工具链）
- **显式**：需要程序员明确知道自己在做什么，没有隐式类型转换
- **组合**：组合优于继承，干脆不允许继承，所有的类都是平级的
- **并发**：内置并发功能
- **面向工程**：快速的构建，严格的依赖树，标准库丰富

## 包

- 一个项目如果要生成一个可执行文件，必须包含一个 `package main`，并且在这个包中至少有一个文件定义了 `main()` 函数。
- 一个目录中的所有 Go 文件必须属于同一个包，不能在同一个目录中存在多个 `package` 声明。
- 综合以上两点，一个项目中只能有一个 `package main`，但可以有多个 `package`，`main` 函数只能在 `package main` 中声明一次。
- Go 语言使用首字母大小写来区分导出和未导出的标识符，没有其他方式！

## 变量

### 声明和类型

- 声明在前，类型在后。
- 允许使用短赋值语句 `:=` 声明一个 `var`，类型将会被自动推断，取决于常量的精度或使用的变量的类型。
- 包级别只能使用声明， 且不允许 `:=`。
- Go 语言不存在隐式类型转换。

```Go
// 变量块声明
var (
	x int = 1
	y string
	z bool
)

var a, b, c int = 1, 2, 3

// 常量块声明
const (
	Pi    = 3.14
	Truth = true
)

var i = 1 // 类型推断为 int
j := i // 错误
```

### 基本类型

- Go 支持复数，`a + bi`，其中 `a` 可有可无，`b` 必须存在。
- Go 内置高精度

### 其他类型

- Go 支持指针，但没有指针运算
- 结构体 `type [结构体名] struct {}`
    - 以相同的语法使用指针或引用访问结构体字段：`(*p).X` `p.X` `a.X`
    - 初始化：`var v1=Vertex{1,2}, v2=Vertex{X=1}, p=&Vertex{}`
- 数组 `var a [10]int`，值语义
    - 切片 `b := a[1:4]`，引用语义，左闭右开
    - 切片有默认上下界
    - 可以用 `range` 形式的 `for` 遍历数组，注意这里的 v 是值的拷贝
        ```go
		var pow = [...]int{1, 2, 4, 8, 16, 32, 64, 128} // 试试去掉中括号里的 ...

		func main() {
			fmt.Printf("%T\n", pow)
			for i, v := range pow {
				fmt.Printf("2**%d = %d\n", i, v)
			}
		} 
        ```
    - 二维切片
        ```go
		picture := make([][]uint8, dy)
		for i := range picture {
			picture[i] = make([]uint8, dx)
		}
        ```
- 映射 `map[键类型]值类型`

### 闭包

```Go
func adder() func(int) int {
	sum := 0
	defer func() { sum++ }()
	return func(x int) int {
		sum += x
		return sum
	}
}

func main() {
	pos, neg := adder(), adder()
	for i := 0; i < 10; i++ {
		fmt.Println(
			pos(i),
			neg(-2*i),
		)
	}
}
```

如果一个函数类型的值引用了一个其外部的变量，那么这个变量将被`捕获`并在闭包的生命周期内持续存在，在上面的代码中，由于 adder 返回的函数类型的值引用了 `sum`，`sum` 将被分配到堆而不是栈帧。注意 pos 和 neg 对应的闭包所`捕获`的变量是独立的。

## 控制流

### for

- `for [初始化];[条件表达式];[后置] {}` 大括号不可省略， 初始化、条件表达式和后置都可以省略
- 一个只有条件表达式的 for 就是 Go 中的 while
- 一个没有条件表达式的 for 就成为无限循环

### if

- `if [初始化];[条件表达式] {} else {}`

### switch

- `switch [初始化];[条件表达式] {case [value]:{}}`
- `case` 不必是常量，且取值不限于整数
- Go 不会执行**首个匹配**的 `case` 之后的 `case`
- 可以省略条件表达式，这时相当于使用 `true` 作为条件

### defer

- `defer` 语句会将函数推迟到外层函数返回之后执行。
- 参数会立即求值
- 后进先出

### panic-recover

- `panic(message)` 会使进程提前返回到调用者（`defer` 会被执行），并表现为 `panic()`，也就是说，这个过程会持续到调用栈的顶部或被 `recover`
- `recover() message` 捕捉 `panic`，从而中断恐慌过程，正常返回调用者。
- `recover()` 只有在 `defer` 的函数里才有用，因为恐慌过程中只有这些函数会被执行。

## 方法

Go 没有类，但存在一种特殊的函数，声明时绑定一个`接收者`，从而可以为类型定义方法，并用类似于`类`的形式调用。

接收者有两种类型：
- 值类型：“类”以值类型解释，操作不影响接收者原来的值，每次调用都会进行复制操作
- 指针类型：“类”以引用类型解释

对于指针类型接收者，`v.SomeMethod()` 等价于 `(&v).SomeMethod()`。也就是说，`v.SomeMethod()` 的行为可能不同，取决于方法定义时使用的接收者类型。

这种省略只出现在这里，在类型包装到接口里时，类型必须与接口的接收者对应。

```Go
type rot13Reader struct {
	r io.Reader
}

func rot13(b byte) byte {
	if b >= 'A' && b <= 'Z' {
		return 'A' + (b-'A'+13)%26
	}
	if b >= 'a' && b <= 'z' {
		return 'a' + (b-'a'+13)%26
	}
	return b
}

func (rot *rot13Reader) Read(bufferOut []byte) (int, error) {
	n, e := rot.r.Read(bufferOut)
	for i := range bufferOut {
		bufferOut[i] = rot13(bufferOut[i])
	}
	return n, e
}

func main() {
	s := strings.NewReader("Lbh penpxrq gur pbqr!")
	r := rot13Reader{s}
	io.Copy(os.Stdout, &r)
}
```

## 并发

Go 语言的并发模型是基于 CSP（Communicating Sequential Processes）模型的，通过 `goroutine` 和 `channel` 实现。

- `goroutine` 之间可以通过并发安全的 `channel` 通信。
- GO 运行时将多个 `goroutine` 映射到少量 OS 线程上。

```go
type SafeMap struct {
	mu sync.Mutex
	v  map[string]bool
}

// Crawl 用 fetcher 从某个 URL 开始递归的爬取页面，直到达到最大深度。
// Crawl 只是创建了一个共享 map 和一个子函数, 并在子函数间共享已经爬取页面的上下文
func Crawl(url string, depth int, fetcher Fetcher) {
	// 在所有子 goroutine 中共享的 map, 用于记录已经爬取过的 url
	m := SafeMap{v: make(map[string]bool)}

	var do_Crawl func(url string, depth int, fetcher Fetcher)
	do_Crawl = func(url string, depth int, fetcher Fetcher) {
		if depth <= 0 {
			return
		}
		body, urls, err := fetcher.Fetch(url)
		if err != nil {
			// 这里没有更新 map, 所以 Fetch 失败的 url 会再次被尝试爬取
			fmt.Println(err)
			return
		}

		// 临界区, 用于检查 url 并更新 map
		// 锁是不区分读写的版本, 粒度比较大
		m.mu.Lock()
		if m.v[url] {
			m.mu.Unlock()
			return
		}
		fmt.Printf("found: %s %q\n", url, body) // 实际爬取
		m.v[url] = true
		m.mu.Unlock()

		// 递归地爬取子 url
		// 使用 chan 等待子 goroutine 完成,
		// 这会导致树形的爬取过程, 只有等叶子节点返回后, 父节点才会返回
		// 尽管 goroutine 是并发的, 这种方式却在层间导致了类似栈的返回过程
		// 理想的情况应该是除了根节点, 其他节点都不必等待子节点返回, 可以用 sync.WaitGroup 实现
		ch := make(chan int)
		for _, u := range urls {
			go func(url string) {
				defer func() { ch <- 1 }()
				do_Crawl(url, depth-1, fetcher)
			}(u)
		}
		for range urls {
			<-ch
		}
	}

	do_Crawl(url, depth, fetcher)
}

func main() {
	Crawl("https://golang.org/", 4, fetcher)
}
```