# eko 派生服务 —— show、inspect 与相等

状态:语法定案(2026-10)。结论先行,理由随文。
2026-10 增订:`Bytes` 入 inspect 表与 `<` 内建清单(§3/§5);定案见 bytes.md。
2026-10 增订:派生家族添 `to_json/from_json`(裁定 12);定案见 json.md。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 双通道:`show`(显示,可覆写)与 `inspect`(结构,永不可覆写) |
| 2 | `show` 每类型自带:默认 = inspect 减顶层 String 引号;插值走 show |
| 3 | `inspect` 永远派生:完全字面形,所见可所写,零例外 |
| 4 | 覆写:声明模块在类型命名空间写 `Money.show` 即遮蔽默认;所有权防孤儿;inspect 无此通道 |
| 5 | `==` 全数据型派生:结构深相等;capability 与函数无 `==` |
| 6 | `<` 仅 ADT 派生:变体声明序,载荷递推;记录与 Bool 无 `<`;内建手工定义 |
| 7 | 相等与比较两边同型:泛型 `(t, t) -> Bool` 锁死;`1 == 1.0` 是类型错误 |
| 8 | Float:`==` IEEE(`NaN != NaN`);show 最短往返十进制;`NaN`/`Inf` 如实 |
| 9 | show/inspect 是编译期泛型分派(type.md §5):裸形 `show(x)` 按静态类型落其命名空间(`Money.show(m)`);接收者形 `x.show()` 同站 |
| 10 | 字典保插入序;show 与迭代随此 |
| 11 | 记录类型自带 `with`:`p.with({ age: 37 })` 部分字段字面量更新;类型不可表达,编译器内建 |
| 12 | `to_json`/`from_json` 默认派生:同构表与外标形见 json.md;覆写机器同 show(声明模块所有) |

## 1. 双通道

单通道覆写会丢结构真相(Java `toString` 之坑);Python 与 Rust 各自独立
长出双通道。eko 一次裁齐:

| 通道 | 名 | 语义 | 可覆写 | 使用处 |
|---|---|---|---|---|
| 显示 | `show` | 给人读 | ✓(命名空间声明) | **插值 `"{e}"`**、日志 |
| 结构 | `inspect` | 保真字面形 | ✗ 永远派生 | 诊断、测试、调试 |

两个通道都是零类型自由函数(裸形 `show(t)`,module.md 裁定 13 同款
入域)与类型命名空间成员(`t.show()`、`Money.show(m)`)双形可用。

## 2. show:默认与覆写

默认派生 = inspect 减**顶层 String 引号**——顶层裸形是插值逼的:
`"hi {name}"` 必须出 `hi wd`,而 `Out.println` 只吃 String,插值即
显示主通道。复合体内的 String 带引号(`{name: "wd"}` 消歧)。

覆写走既有机器,零新语法:

```eko
Money: type = [cents: Int, cur: String]

Money.show: (m: Money) -> String = "{m.cur} {m.cents}"

m: Money = {cents: 12500, cur: "USD"}
"{m}"          // USD 12500           覆写生效
inspect(m)     // {cents: 12500, cur: "USD"}   结构永真
```

- 覆写是命名空间声明,所有权归声明模块(module.md §4.3),无孤儿
  覆写;未覆写的类型零成本走默认。
- 覆写让 `"{m}"` 的输出取决于类型声明——这是插值唯一一处非本地
  推理,代价诚实记录;结构通道永远在场兜底。

## 3. inspect:字面形

所见可所写,零例外(与 io-world §5.3 `_` 的显示精神同源):

| 类型 | inspect | show(未覆写) |
|---|---|---|
| Int/UInt | `3` | 同 |
| Float | 最短往返:`0.5`、`3.14159`;`NaN`、`Inf` | 同 |
| Bool | `True`、`False` | 同 |
| String | `"wd"`(带引号) | `wd`(顶层裸) |
| Char | `'a'`(带引号) | `a`(顶层裸) |
| Bytes | `Bytes.from_hex("4142")`(bytes.md) | 同 |
| 列表/元组 | `{1, 2, 3}`、`{"first", 2}` | 同 |
| 字典/记录 | `{"1": 1}`、`{name: "wd", age: 2}` | 同 |
| ADT | 构造子应用形:`Man`、`Some 3`、`Err "…"` | 同 |
| Unit | `_` | 同 |
| capability | `<cap>`(io-world §2) | 同 |
| 函数 | `<fn>` | 同 |

空容器 `{}`;记录字段序 = 声明序;字典序 = 插入序(裁定 10)。

## 4. ==:全数据型派生

- 结构深相等:记录逐字段、ADT 同构造子且载荷递推、容器逐元素;
  `Option`/`Result` 随 ADT。
- 例外两个:capability 无 `==`(io-world §2 既有),函数无 `==`
  (闭包比较无意义)。
- Float IEEE:`NaN == NaN` 为 `False`——硬件诚实,如实记;需要
  全序语义将来走 `Float.total_eq` 函数,不入运算符。
- 两边同型:`Int.eq: (Int, Int) -> Bool` 签名锁死;`age == n`
  (UInt vs Int)是类型错误,转换显式(`Int.to_float(x) == y`,convert.md)。

## 5. <:有序是声明语义

- ADT 派生 `<`:变体按**声明序**,载荷按序递推(`Sex: Man | Woman`
  ⟹ `Man < Woman`)——声明列表天然是次序,派生即如实。
- 记录**无 `<`**:字段集没有大小语义,强派序是任意的;排序用
  `sort_by` 显式比较子(io-world §10 既有)。
- Bool **无 `<`**:`True | False` 的次序是枚举便利,不是大小;
  分区用 `filter`。
- 内建(数值、Char 码点序、String 字典序、Bytes 字节序)手工定义,不走过场。

## 6. with:记录的更新服务

记录类型自带 `with`——派生服务家族的更新员:

```eko
p2       := p.with({ age: 37 })
invoice2 := invoice.with({ status: Paid, paid_at: now })
p3       := p.with({ address: { city: "cd" } })   // 深更新:嵌套部分字面量
p.with({ age: 37 }).notify(out)                   // 直链;调用即原子
```

- 实参是**部分字段字面量**:仅在 with 的实参位合法,逐层对照字段
  类型受检(错拼即错误)——type.md §3.2"复合字面量永不独行"的
  唯一定点例外。
- `Person.with: (Person, 部分字段) -> Person` 的第二参类型在 eko 类型语法中
  **不可表达**——全语言第一个类型不可表达的编译器内建服务,魔法
  明码标价一处。
- 旧值不动(值不可变);词汇分立:`with`(记录更新)、
  `insert`/`remove`(字典)、`append`/`merge`(`++` 的本体,
  expr.md §1)。

## 7. 泛型分派与速查

show/inspect/`==`/`<` 皆编译期泛型分派(type.md §5):裸形按静态
类型落类型命名空间,派生默认是分派的缺省成员;位零参与(全纯)。

```
show(t)            ≡  t.show()          // 分派,覆写或缺省
inspect(t)         ≡  t.inspect()       // 永远派生
a == b             ≡  Int.eq(a, b)      // 同型锁死;按静态类型落命名空间
a < b              ≡  Int.lt(a, b)      // ADT/内建
"{e}"              ≡  show(e) 注入      // 显示通道
p.with({age: 37})                       // 记录更新,派生服务
```
