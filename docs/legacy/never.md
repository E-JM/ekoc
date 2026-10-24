# eko Never —— 底类型与必不返回

状态:语法定案(2026-10)。结论先行,理由随文。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | `Never: type =` prelude 零变体 ADT,零值;用户亦可声明零变体型 |
| 2 | `-> Never` = 必不返回;函数自身可作值传递,调用结果永不可得 |
| 3 | 底类型:`Never <: t`,挂进位子类型同一适配位置(实参、操作数、期望类型、集合元素、臂、声明返回位)——eko 值域唯一子类型 |
| 4 | 适配只在值位;函数类型内部不适配(`(a) -> Never` 不适配 `(a) -> t`);lambda 字面经推断 `u := Never` 直达 |
| 5 | **穷尽靠模式,不靠发散**;Never 臂不顶穷尽 |
| 6 | Never 调用后的代码不可达:合法,出警告 |
| 7 | 位无关:任何箭头的合法返回(panic 纯 `-> Never`,exit 涉世 `!-> Never`);`-> t` 不承诺终止(B4 重申) |
| 8 | 写法:递归(`spin: () -> Never = spin()`)或 panic/另一 Never 调用;`<-` 不适用(无末位回调) |

## 1. 身份

```
Never: type =          // 零变体——| 列表的退化形,语法零新增
```

与 Bool 同款 prelude ADT 身份;零构造器,零值。与 Unit 泾渭分明:
`_` 恰有一值,Never 零值(io-world §5.3 已警告勿混)。**函数是值,
结果不是**:`never_fn` 可传递可引用,`never_fn()` 求值不停在返回上。

## 2. 底类型适配

`Never <: t` 使 panic 一类能落在任意表达式位:

```eko
x := if c: compute() else: die("no")     // Never 臂适配 Int
case r:
    Ok v  -> v
    Err e -> die("{e}")                   // 同
```

适配位置与位子类型(io-world §5.1 规则 3)同一清单,增两处:**臂**
与**声明返回位**。eko 的全部子类型就此两条——位适配 + Never 底
类型,有界可教学。

边界:适配只在**值位**。函数类型内部不适配——具名
`g: (a) -> Never` 传不到期望 `(a) -> Int` 的实参位(与 `?` 的位置
纪律同精神,io-world §5.2:那里没有绑定在泛化,只剩假精确);逃生
是包一层 lambda,而 lambda 字面本就经推断 `u := Never` 直达,无需
子类型:

```eko
List.each(xs, (s): die("no head"))       // 回调类型 (String) -> Never,
                                          // u := Never,推断即达
```

## 3. 穷尽靠模式,不靠发散

一个臂上两件差事,归功对象不同:

| 差事 | 谁干的 |
|---|---|
| 穷尽被闭合(编译期保证) | **模式**(`Err e`、`_`) |
| 臂的类型与 case 之型相容 | **Never 身体**(§2 适配) |

"分支不正常返回"只解释第二件。若让发散顶穷尽:

```eko
case light:
    Red -> stop()
    panic("unreachable")     // ✗ 此形不存在:裸 never 不是臂
```

`Green` 未被任何模式处理却通过编译——穷尽从编译期保证降级为运行
时崩溃,保证空心。故臂永远是 `PATTERN -> 表达式`(branch.md §2),
惯用式:

```eko
case r:
    Ok _ -> proceed()
    _    -> panic("unreachable")     // 模式 _ 盖面,panic 管型
```

穷尽性按构造子计(branch.md 裁定 3),不看居留性——`Result t Never`
仍配 Err 臂;编译器不证明"不可居留",规则一句,可预测。

## 4. 线性流与位

```eko
main: (out: Out) ! =
    out.println("start")
    die("boom")
    out.println("never")     // 不可达:合法,出警告
```

不可达出**警告**不是错误;诊断三级:错误/警告/无(2026-10 修订,
警告通道解禁)。位无关:Never 是任何箭头的合法返回,panic 纯
`-> Never`,将来的 Proc.exit 涉世 `!-> Never`,位照常由体判定。
`-> t` 不承诺终止:发散不入位、不被追踪(B4 非承诺重申),Never
标注的是"必不返回",不是"可能不返回"。

## 5. 写法与速查

```eko
spin: () -> Never = spin()                 // 递归即发散;递归绑定自由
die: (msg: String) -> Never = panic("[die] {msg}")

xs: []Never = {}                           // 零值直推:元素永无,只有空
f: (t) -> Never                            // 合法签名
g: (f: (Int) -> Never, n: Int) -> Int =    // 具名传递合法(值!)
    f(n)                                   // 调用式 Never,适配 Int
```

- `<-` 不适用于 Never 调用(回调契约,bind.md §3.1)。
- `show`/`==` 对 Never 平凡成立——无值可到。
