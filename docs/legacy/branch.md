# eko 分支 —— case 与 if

状态:语法定案(2026-10)。结论先行,理由随文。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 分支原语是 case;if 是 case-on-Bool 的排版糖 |
| 2 | case 臂:`PATTERN -> 表达式`;scrutinee 求值一次,自上而下,首个命中 |
| 3 | 穷尽:代数型配齐全部构造器,或以 `_` 通配臂收尾 |
| 4 | 卫式臂:`PATTERN if GUARD -> …`;卫式臂之后以无卫臂收尾 |
| 5 | if 两臂同型;省略的 else 即 `else: _`;条件是 Bool |
| 6 | 单行形:`;` 分隔多个表达式与多个臂 |
| 7 | 位透明:条件与分支的位照常 OR 进外围 |
| 8 | 多 scrutinee 是元组 case 的糖:`case a, b: P, Q -> R` ≡ `case [a, b]: [P, Q] -> R`(pattern.md §5) |

## 1. case:分支原语

```eko
parse_sex: (data: String) -> Result Sex String =
    case data:
        "man"   -> Ok Man
        "woman" -> Ok Woman
        _       -> Err "unknown sex: {data}"
```

- 臂是 `PATTERN -> 表达式`;scrutinee 求值一次,自上而下匹配,首个命中的臂生效。
- 穷尽:代数型 scrutinee 配齐全部构造器(type.md),其余类型以 `_` 通配臂收尾;`_` 匹配一切(io-world.md §5.3)。
- 臂的模式绑定名字,在结果表达式(及卫式,§2)里可用。
- 多 scrutinee:`case a, b: P, Q -> R` 是元组 case 的糖(pattern.md
  §5),各 scrutinee 求值一次。

case 是表达式,产出命中臂的值;臂即 case 的块(layout.md §4)。

## 2. 卫式臂

```eko
case n:
    n if n < 0  -> "negative"
    n if n == 0 -> "zero"
    _           -> "positive"
```

- `PATTERN if GUARD -> …`:模式绑定先于卫式;卫式是 Bool 表达式,可使用绑定名。
- 卫式臂之后以**无卫臂收尾**——穷尽性在无卫臂处闭合,无卫臂收口整个 case。
- `if` 一词两槽:表达式头(§3)与卫式槽,词汇同一。

## 3. if:case-on-Bool 的糖

```
if c: x else: y   ≡   case c: True -> x
                         False -> y
```

```eko
n = if x < 0: 0 else: x
```

- 条件是 `Bool`,无 truthy 强转;求值一次,只走命中分支。
- 两臂同型;省略的 else 即 `else: _`。Never 臂适配于另一臂之型(never.md §2):

```eko
if err_count > 0:
    out.println("errors!")       // then 也是 _,两侧同型
```

## 4. 单行形

`;` 分隔单行内的多个表达式,也分隔单行的多个 case 臂:

```eko
str = if n < 0: "negative" else: "other"
case r: Ok v -> v; Err e -> 0
if c: x(); y() else: z()         // then 块两条表达式
```

## 5. 布局

if/else 与 case 的块遵循 layout.md:`:` 开块,成员同列,嵌套进一级;else 是 if 的第二个块,与 if 同列起头。

## 6. 位:透明

分支构造对位零特殊规则:条件、卫式、各分支的位照常 OR 进外围(io-world.md §5.1 规则 1):

```eko
if fs.exists(path):              // 条件置位 → 外围 !
    …
```

## 7. 形状选择(注)

| 场景 | 构造 |
|---|---|
| 二元分支 | `if c: x else: y` |
| 同一值的多路 | `case n:` + 卫式 |
| 异条件多路 | 嵌套 if |
