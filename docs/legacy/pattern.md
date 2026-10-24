# eko 模式 —— 全谱

状态:语法定案(2026-10)。结论先行,理由随文。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 模式是组合文法:变量、`_`、字面量、构造器、元组、记录、列表、or;嵌套无深度限制 |
| 2 | 记录模式子集匹配:写谁绑谁,字段名受检,简写与改名并存 |
| 3 | or-模式 `P \| Q`,各支绑定同名同型;`\|` 模式位专用 |
| 4 | 列表模式五形:空 `{}`、定长、后缀 rest `{h, ..t}`、前缀 rest `{.., 42}`、匿名 `{_, ..}` |
| 5 | 字面量模式:字符串/整数/浮点/负号/零载荷构造器;插值串不入 |
| 6 | 同一模式绑定名不重复;`_` 任意位可重复 |
| 7 | 字典模式、as-模式永禁;lambda/`<-` 参数位维持标识符与元组 |
| 8 | 卫式只在臂级(branch.md §2 既有) |

## 1. 记录模式

子集匹配——模式写到的字段才绑定,其余忽略;字段名对照记录类型
**受检**,错拼即编译错误,子集不会静默漏配:

```eko
case r:
    Ok {name, age}        -> greet(name, age)   # 简写:字段名即绑定名
    Ok {name: n, age: a}  -> "{n}:{a}"           # 改名
    Err msg               -> log(msg)
```

## 2. or-模式

```eko
case ev:
    Key Up | Key Down  -> nav(ev)                # 各支绑定同名同型
    Key Enter          -> submit()
```

`|` 与类型位的代数分隔符分位而立(type.md §2),不撞;值位无 `|`。

## 3. 列表模式

```eko
case xs:
    {}                 -> empty()
    {x, y}             -> pair(x, y)             # 定长:表或元组,期望类型裁定
    {h, ..t}           -> walk(h, t)             # 后缀 rest
    {.., 42}           -> ends_42()              # 前缀 rest
    {_, ..}            -> non_empty()            # 匿名 rest
    {Foo, Count n, ..} if n > 0 -> …             # 组合:构造器+载荷+卫式
```

元组 ≥2 元（type.md §3.1），`{}` 只读空表；rest 绑尾表（或头表），
前缀后缀同权。无 loop 语法，自定义列表递归以此为骨（例档
`~/Work/example/pattern_match_list.md` 即此谱；栈保证与形态对译
见 loop.md）。

## 4. 组合与禁项

- 嵌套任意:`Ok {addr: {city}}`、`{Some x, ..rest}`。
- 同一模式绑定名不重复:`(x, x)` 编译错误;`_` 例外(通配不绑定,
  任意位可重复)。
- **字典模式永禁**:键是值不是结构,取值走 `Dict.get -> Option`,
  天然分支。
- **as-模式永禁**:要整体,嵌套 case 或字段取用即是。
- lambda/`<-` 参数位 = 标识符与元组参数组(`(p, i):` 即二元参数组,
  type.md §3.3;非自动解构,拆元组值走 case 模式或绑定行);记录、
  构造器、列表模式不进参数位。

## 5. 多 scrutinee(糖)

```
case a, b:  P, Q -> R     ≡     case [a, b]: [P, Q] -> R
```

```eko
case n % 3 == 0, n % 5 == 0:
    True,  True  -> "FizzBuzz"
    True,  False -> "Fizz"
    False, True  -> "Buzz"
    False, False -> show(n)
```

逗号镜像元组(type.md §3.3 同源);各 scrutinee 求值一次,臂内照常
卫式。branch.md §1 同款裁定。

## 6. 速查

```
Ok {name, age}         # 记录子集 + 简写
Err "timeout"          # 字面量嵌套
Key Up | Key Down      # or
{h, ..t}  {.., 42}  {_, ..}     # 列表 rest 三形
case a, b: P, Q -> R   # 多 scrutinee 糖
```
