# eko 取值 —— get 唯一入口与钻取

状态:设计定案(2026-10)。结论先行,理由随文。
本文撤销 expr.md"x[i] 越界 panic 既有裁定"与 type.md §3.1"类型形即取值形";
旗舰例(bind.md、module.md)同步重写。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 索引语法 `x[i]` 永禁:表达式位 `[` 退役,类型位独占;结构取值一律函数 |
| 2 | 三入口各一:`List.get: ([]t, Int) -> Option t`、`Dict.get: ([k]v, k) -> Option v`(O(1));`String.at: (String, Int) -> Option Char`(O(n) 明码)——成本进名字:get O(1)、at O(n) |
| 3 | 信任在消费处声明:`expect/unwrap`(断言)、`to_result`(Result 桥)、`unwrap_or`(默认)——取值处不选通道 |
| 4 | `Option.get: (Option ([]t), Int) -> Option t`——列表钻取,prelude 普通函数,零魔法:`m.get(i).get(j)`;住在 Option 命名空间(站 2 链式解析) |
| 5 | 钻取主通道 = `<-` + `Option.to_result`——Rust `?` / Gleam `use` 的同构,零 lambda |
| 6 | 越界 panic 整个消失:索引即域检查,正中三分法(convert.md §1) |
| 7 | 字典载荷钻取(`Option ([k]v)`)另立若痛;异构导航(JSON)走 case 变体——名义型必经 |
| 8 | 禁:索引语法、签名重载、`Option.get` 载荷分派内建、`get_or` 复合词 |

## 1. 一入口

"下标在不在域内"是有无之问——域检查,Option 是标准答案(convert.md
三分法)。`xs[0]` 的旧故事是取值处静默承诺不变量,崩点远离病因:旗舰例
`data[1]` 对短行 panic 而非 Err,三分法违例面就在语料里。

结构取值全家,每个结构恰好一个入口:

| 结构 | 入口 | 形 |
|---|---|---|
| 记录 | 字段 | `p.name`(call.md 站 1) |
| 元组 | 模式 | `(a, b) = p`、case 臂 |
| 列表 / 字典 | `get` | `xs.get(i) -> Option t`、`d.get(k) -> Option v` |
| 字符串 | `at` | `s.at(i) -> Option Char`,O(n) 明码(char.md) |

表达式文法失去最后一处 `[`——两对括号各安其域:类型位 `[...]`,
值位 `{...}`,零重叠(type.md §3)。

## 2. 信任在消费处

取值恒产 Option;声明信任是消费端的事,显式、带消息:

```eko
v = xs.get(i).expect("i 已验")            // 断言,知情 panic
v = xs.get(i).unwrap_or(0)                // 默认证人
age <- data.get(1).to_result("短行").try()  // 进 Result 链
```

旧双轨(`[]` panic + `.get` Option)把信任声明藏在**选语法**里;
新故事与 Option 自身同构——一语法,消费处分岔(case / expect),
不是两语法各硬连一通道。越界 panic 因此从语言整个消失,panic 面
收缩为:算术(除零、溢出)、unwrap/expect、panic、assert。

## 3. 钻取

| 场景 | 形 |
|---|---|
| 列表钻取(表达式位) | `m.get(i).get(j)`——`Option.get`,None 透传 |
| 语句级(Result/宿主) | `<-` + `to_result`:每层一句,零 lambda |
| 已验不变量(算法) | `m.get(i).expect("i").get(j).expect("j")`;或扁平下标 `grid.get(i * w + j)` |
| 默认(表初始化) | `m.get(i).get(j).unwrap_or(0)` |
| 异构(JSON) | case 变体导航——名义型必经;穿结构用 `and_then` |

`Option.get` 是 prelude 普通函数,用户皆可自写,零魔法:

```eko
Option.get: (rows: Option ([]t), i: Int) -> Option t =
    case rows:
        Some xs -> xs.get(i)
        None    -> None
```

住在 **Option 命名空间**:`m.get(i).get(j)` 的接收者类型是
`Option ([]t)`,站 2 按头型查 Option 命名空间(call.md §2)——放
List 命名空间则链断,只剩直呼。字典载荷版同名即撞(签名重载,
裁定 8);载荷分派是编译器内建,无先例,否决——嵌套字典今日走
`and_then`,若痛再立。

`<-` 是钻取主通道(bind.md §3),Rust `?` 与 Gleam `use` 的同构:

```eko
row <- m.get(i).to_result("no row").try()
v   <- row.get(j).to_result("no col").try()
```

## 4. 先例对照(记)

多数派双轨(Rust/Swift/Go/C#)保 `[]` + panic,Swift 的"字典安全、
数组崩"是双政策内部张力的样本。血亲派(Gleam/Elm)函数唯一,钻取靠
`use`/`and_then`——eko 从之。JS/Kotlin 的 `?.[]` 语法安全导航是
C4 已禁的后缀形,且都建立在 null/undefined 上;Ruby/Julia 的宽松
默认与诚实不合。

## 5. 禁止清单

- **`x[i]` 索引语法(一切结构)**——两套取值词汇;取值处静默信任。
- **签名重载**——同名不同签名是隐藏 ad-hoc 多态,与泛型分工相抵
  (开放多态 = 参数化泛型,ad-hoc = 封闭 union + case,generics.md §3.4)。
- **`Option.get` 载荷分派内建**——无先例;derive.md §6 `with` 之后
  不添第二个类型不可表达内建。
- **`get_or` 一类复合词**——`get` + `unwrap_or` 可组合,不配词。

## 6. 速查

```
xs.get(i)                       // Option t;唯一入口,O(1)
d.get(k)                        // Option v
s.at(i)                         // Option Char,O(n) 明码
m.get(i).get(j)                 // 钻取链:Option.get,None 透传
row <- m.get(i).to_result("…").try()   // 主通道:<- + to_result
xs.get(i).expect("已验")        // 断言;unwrap_or(0) 默认
x[i]                            // ✗ 永禁;表达式位 [ 已退役
```
