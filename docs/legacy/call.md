# eko 调用与成员解析 —— 接收者形(UFCS)

状态:语法定案(2026-10)。结论先行,理由随文。
2026-10 修订:跨模块全名与接收者扩展随 module.md 定案(文件即模块、
类型/构造器随 import 进非限定域、UFCS 限定路径)。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | `a.f(b)` ≡ `f(a, b)`:第一个参数即接收者,两种拼写恒等 |
| 2 | `.` 统一承担字段访问与函数查找 |
| 3 | 查找次序:记录字段 → 类型命名空间 → 当前模块 → 模块路径 |
| 4 | 跨模块函数以全名调用;接收者位同权:`x.Mod.f(args)` ≡ `Mod.f(x, args)` |
| 5 | 多步变换由接收者链承担,链式场景并入 `.` |

## 1. 一条等式

```
a.f(b, c)  ≡  f(a, b, c)
```

两种拼写永远同时可用,语义恒等。限定名同权(module.md §4.1):
`a.Mod.f(b, c) ≡ Mod.f(a, b, c)`。语言中的函数皆为自由函数,挂在
类型命名空间或模块之下;`.` 是它们在调用点的接收者形。

## 2. prelude 是接收者形的天然土壤

io-world.md 的全部签名以能力、句柄或数据为第一个参数,接收者形与之严丝合缝:

```eko
out.println("hi")           // ≡ Out.println(out, "hi")
fs.open("data.csv", Read)   // ≡ Fs.open(fs, "data.csv", Read)
file.read(4096)             // ≡ File.read(file, 4096)
xs.map((n): n * 2)          // ≡ List.map(xs, (n): n * 2)
line.split(",")             // ≡ String.split(line, ",")
```

世界调用(`out`/`fs`/`file`)、容器遍历(`xs`)、纯变换(`line`)三者同一形状。

## 3. `.` 的查找次序

`x.f(args)` 依次在三个站点解析,命中即止;`f` 为多段限定名时另有
第 4 站:

1. **记录字段**:`p.name` 取记录 `p` 的 `name` 字段。
2. **类型命名空间**:在 `typeof(x)` 的命名空间中找 `f`。内建容器的对应关系:`[]t → List`、`Option t → Option`、`Result t e → Result`。命名空间归声明该类型的模块所有,随类型旅行(module.md §4.3)。
3. **当前模块**:当前文件(module.md §2)顶层以 `typeof(x)` 为首参的自由函数。
4. **模块路径**:限定段 `x.Mod.f(args)`(module.md §4.1);接收者位永不查词法绑定。

同一名字既是字段又是函数时,编译器列出两者,作者以字段访问或全名调用择一;字段名撞模块名同此机制(module.md §4.2)。

## 4. 跨模块:全名与接收者扩展

import 声明依赖并开域:类型与构造器进非限定域,函数永远全名
(module.md §3):

```eko
Csv.parse_line(l)
Net.get(net, url)
csv.lex.tokenize(l)     // 全名即路径,无包含关系(module.md §2)
```

接收者位同权——UFCS 恒等式对限定名的延拓(module.md §4.1):

```eko
persons
    .filter((p): p.age >= 18)
    .stats.summary()    // ≡ stats.summary(persons.filter(...))
```

全名让来源在调用点可见;`.` 的三个查找站点因此边界清晰。

## 5. 链式

多步变换即接收者链,每一环的接收者是上一环的值;跨模块环节以
`.Mod.f()` 接入(§4):

```eko
content.split("\n")
       .map(String.trim)
       .filter((l): l != "")
       .length()
```
