# eko 字符 —— Char 与 String 元素域

状态:语法定案(2026-10)。结论先行,理由随文。
2026-10 增订:字节域经 Bytes 桥定案(bytes.md);§4 注记同步。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | `Char` 是 prelude 标量:Unicode 标量值(USV),域 0..0x10FFFF 除代理对 |
| 2 | 字面量 `'c'` 单引号;转义同 String 加 `\'`;非 ASCII 直书 UTF-8 |
| 3 | show/inspect 与 String 平行:顶层裸 `a`,字面形 `'a'` |
| 4 | `==`/`<` 内建,码点序 |
| 5 | **String 无 `s[i]` 索引**:变宽编码 O(1) 索引是谎言;`String.at -> Option Char` O(n) 明码;索引语法全语言永禁(access.md) |
| 6 | `String.length` 数标量值,不数字节 |
| 7 | 桥:`String.chars -> []Char`、`String.from_char -> String`、`String.from_chars -> String`、`Char.to_code -> Int`、`Char.from_code -> Option Char` |
| 8 | Char 不是单元 String:两型不互换,转换显式 |

## 1. 身份:为什么是标量值

Unicode 三层级:字节(二进制域)、**标量值**(域固定)、字形簇(变长
序列)。标量类型要域固定——一个 Char 恰是一个码位;字形簇是序列
性质,归 String 函数族(`graphemes` 另立若痛)。Swift 变长
Character 的索引拧巴是前车之鉴。

## 2. 字面量与显示

```eko
c: Char = 'a'
zh: Char = '中'           // 非 ASCII 直书
nl: Char = '\n'           // 转义同 String 加 \'
```

show('a') = `a`(顶层裸,插值通道);inspect('a') = `'a'`(字面形)
——与 String 一条规则平行成对(derive.md §3)。

## 3. String 元素域:索引的诚实

UTF-8 变宽,O(1) 索引物理不成立:

- `s[i]` **不存在**(索引语法全语言永禁,access.md;列表/字典亦走 `get`,字符串另有 O(n) 明码 `at`)。
- `String.at: (String, Int) -> Option Char`——O(n) 明码,越界
  `None`。
- `String.length: (String) -> Int` 数标量:`"héllo".length() == 5`
  (Rust 数字节的陷阱不设)。
- 字符级遍历走桥:`"abc".chars().map(f)`——chars 产 `[]Char`,
  与 List 命名空间组合,零 String 专属迭代函数。

## 4. 桥与边界

```
String.chars:      (String) -> []Char
String.from_char:  (Char) -> String         // 单字符
String.from_chars: ([]Char) -> String       // 批量
Char.to_code:      (Char) -> Int            // 码位
Char.from_code:    (Int) -> Option Char     // 非法码位 None
```

- Char ≠ 单元 String:`'a' ++ "bc"` 是类型错误;转换显式。
- Range 域维持 Int/UInt,`'a'..'z'` 另立若痛;`'a' <= c && c <= 'z'`
  今日足够。
- 字形簇族、`\u{…}` 另立若痛;字节域经 Bytes 桥——`s.to_bytes().length()`
  (bytes.md),`byte_length` 不配词。

## 5. 速查

```
'中'                      // Char 字面量,UTF-8 直书
"{c}"                     // a        show 顶层裸
inspect('a')              // 'a'      字面形
"abc".at(1)               // Some 'b' O(n) 明码
"héllo".length()          // 5        数标量
"abc".chars()             // {'a', 'b', 'c'}
String.from_char('a')     // "a"
'a'.to_code()             // 97
Char.from_code(65)        // Some 'A'
```
