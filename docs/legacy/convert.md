# eko 转换 —— 失败语义与 parse 族

状态:语法定案(2026-10)。结论先行,理由随文。
2026-10 修订:`UInt.to_int` 改为 Option(Int64 不覆盖 UInt64 全域);`Float.to_uint` 负小数先判域;Float NaN 往返验收按 NaN 等价(不承诺 payload 位)。
2026-10 增订:ParseError 复用于 UTF-8 解码与 hex 解析(bytes.md)。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 三分法:输入验证走 `Result`(带为什么),域检查走 `Option`(有无),转换永不 `panic` |
| 2 | 全函数族:`Int.to_float`/`UInt.to_float`(`>2^53` 最近舍入,如实记)、`Char.to_code`、`String.from_char` |
| 3 | Option 族:`UInt.to_int`(`> Int.max` None)、`Int.to_uint`(负值 None)、`Float.to_int`/`Float.to_uint`(非有限或越界 None;小数截断非失败,与 `/` 同基;负小数入 UInt 先判域) |
| 4 | Result 族:`Int/UInt/Float.parse: (String) -> Result t ParseError`;`ParseError` 为 prelude 封闭 ADT |
| 5 | 断言逃生:`expect`/`unwrap` 纯(io-world §3),知情显式 |
| 6 | 往返性质:`parse(show(x)) == Ok x`(非 NaN;Int/UInt 精确,Float 由最短往返保证);NaN 按 NaN 等价验收 |
| 7 | 命名全拼:`to_int`/`to_uint`/`to_float`——域词全拼,缩写属机界(数值命名裁定同源);尺寸族转换与进制 parse 随 FFI 波次另立 |

## 1. 三分法

失败三性对三词,各就其位:

| 失败的性质 | 词汇 | 先例 |
|---|---|---|
| **输入验证**:外部数据不可信,要带为什么 | `Result` | `IoError`(io-world §3) |
| **域检查**:问有无,不问原因 | `Option` | `Char.from_code`(char.md) |
| **不变量违反**:程序员自己的错 | `panic`(B4) | **转换永不属于此类** |

主轴一句:**输入走 Result,域检查走 Option,转换永不 panic**。知情
断言显式用 `expect`(纯,io-world §3)。

## 2. 家族表

| 函数 | 签名 | 语义 |
|---|---|---|
| `UInt.to_int` | `(UInt) -> Option Int` | 超过 `Int.max` 的 UInt 为 None——Int64 不能无损容纳 UInt64 全域 |
| `Int.to_uint` | `(Int) -> Option UInt` | 负值 None |
| `Int.to_float` / `UInt.to_float` | `(Int) -> Float` / `(UInt) -> Float` | 全函数;`>2^53` 最近舍入(精度损失不是失败) |
| `Float.to_int` | `(Float) -> Option Int` | NaN/Inf/越界 None;**小数向零截断非失败**(`3.7 → 3`,`-0.5 → 0`)——与 `/` `%` 同基(expr.md §2) |
| `Float.to_uint` | `(Float) -> Option UInt` | NaN/Inf/越界 None;**负值先判域**(`-0.5 → None`,`-0.0` 视为 0);非负小数向零截断非失败——与 `/` `%` 同基 |
| `Int.parse` / `UInt.parse` / `Float.parse` | `(String) -> Result t ParseError`(t=各自数值型) | 输入验证;`UInt.parse(age_s).map_err(...)` 语料用法严丝合缝 |
| `Char.to_code` / `String.from_char` | `(Char) -> Int` / `(Char) -> String` | 全函数;char.md 桥族补 singleton 位 |

`Float.to_int`/`Float.to_uint` 与 Rust `as` 的饱和转换划清界限:NaN→0、
Inf→MAX 是无声说谎;eko 的 Option 拒绝谎言。`Float.to_uint` 先判负值域,
负小数不截断成 0——那会丢掉符号,是另一种说谎。

## 3. ParseError

```
ParseError: type = Empty | Invalid String | OutOfRange
```

沿 IoError 判据(调用方会不同分支处理才配得名):Empty 提示必填、
Invalid 报烂输入、OutOfRange 报超限。封闭,不回流。

## 4. 往返性质(验收面)

```
parse(show(x)) == Ok x        // 非 NaN:Int/UInt 精确;Float 由 show 最短往返保证
                              // NaN 例外:IEEE == 下 NaN != NaN,验收按 NaN 等价判定
```

parse 与 show 互为逆——显示通道(derive.md)与输入验证首尾相接,
`"age: {e}"` 报错链闭环。Float 的 NaN 是 IEEE 相等的例外,不是
show/parse 的例外:往返保持 NaN 身份,不承诺 payload 位相同;
验收不能用 `==`。

## 5. 速查

```
n.to_uint()                // Option UInt;负值 None
u.to_int()                 // Option Int;> Int.max None
f.to_int()                 // Option Int;NaN/越界 None;3.7 → Some 3, -0.5 → Some 0
f.to_uint()                // Option UInt;NaN/越界/负小数 None;3.7 → Some 3
UInt.parse(s)              // Result UInt ParseError
Int.to_uint(x).expect("非负")   // 知情断言,纯
parse(show(42))            // Ok 42
```
