# eko 匿名函数 —— lambda

状态:语法定案(2026-10)。结论先行,理由随文。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | lambda 形:`(参数列表): 体` |
| 2 | lambda 零标注:类型与 world bit 均由上下文和体推断 |
| 3 | 捕获即委托:闭包持有能力值 |
| 4 | 位注解住在具名绑定的箭头上 |

## 1. 形态

```eko
(n): n + 1
(p, i): out.println("{i}: {p}")
(xs): xs.map((n): n * 2).length()
```

体是多行块时随缩进:

```eko
persons.each_with_index((p, i):
    out.println("{i}: {p}"))
```

参数类型由上下文给出;实参在最后检查时收集(io-world.md §5.2)。

## 2. 位:恒推断

lambda 零标注。它的位由体决定——体内调用世界内建(io-world.md §5.1)则位为 `!`,否则为纯。位注解住在具名绑定的箭头上:

```eko
each: ([]String, (String) ?-> _) ?-> _      // 分派者签名
```

传给分派者时,开位按 lambda 实例化:

```eko
main: (out: Out, fs: Fs) ! =
    persons.each((p): out.println(p))
    // lambda 位 = ! → each 以 ! 实例化 → 调用贡献 ! → main 为 !
```

## 3. 捕获即委托

闭包捕获能力值,复制即委托(io-world.md §2)。持有是安静的,调用才发声(io-world.md §5.1 规则 1)——工厂可以构造 `!` 闭包而自身纯:

```eko
mk_logger: (out: Out) -> (Int) !-> Int =
    (n): out.println(show(n)); n + 1
```

捕获了 `out` 的 lambda,位由体内的 `out.println` 决定;捕获本身对位安静。

## 4. 与 `<-` 的关系

反转绑定(bind.md)的 rest 就是回调体;`<-` 是 lambda 的排版形:

```eko
age <- data.get(1).to_result("短行").try()
```

即:

```eko
data.get(1).to_result("短行").try((age):
    rest)
```
