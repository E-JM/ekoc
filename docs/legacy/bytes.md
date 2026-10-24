# eko 二进制域 —— Bytes 与字节 I/O

状态:设计定案(2026-10)。结论先行,理由随文。
本文关闭 need.md 规范洞“二进制域”:Bytes 内建、字节 I/O、UTF-8
编解码一次裁齐,与 FFI 波次解绑——解绑的是域,不是机界(§5)。
2026-10 增订:`Byte`/`[]Byte` 备选否决,`UInt8` 提前同拒(裁定 11 与 §1 否决表)。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | `Bytes` 入 prelude:不可变字节序列,内建复合(与 String 同族);无字面量,构造走函数 |
| 2 | 字节值是 0..255 的 `Int`(域词);机词族 `U8/I8…` 仍随 FFI 波次(expr.md §6 不动) |
| 3 | 服务面:`length/get` O(1) 明码、`append` O(n) 全拷、`to_ints/from_ints` 桥;字节级遍历走桥,与 `String.chars` 同构 |
| 4 | `++` 扩域至 Bytes(连接);`==` 结构深相等、`<` 字节字典序(内建手工) |
| 5 | 编解码三分法:`String.to_bytes` 全函数;`Bytes.to_string` 输入验证(Result + ParseError);`Bytes.from_ints` 域检查(Option) |
| 6 | hex 对:`to_hex` 全函数(小写);`from_hex` Result ParseError;往返 `from_hex(to_hex(b)) == Ok b` |
| 7 | 字节 I/O:`File.read_bytes/write_bytes`、`Fs.read_bytes/append_bytes` 置位;read_bytes 永不 `InvalidData`,短读即 EOF(Ok 短串) |
| 8 | 文本门与字节门分立:`File.read` 非 UTF-8 → `Err InvalidData` 保持;原始字节走 read_bytes——拒绝有正门,不再是死路 |
| 9 | show = inspect = `Bytes.from_hex("…")` 构造子应用形;所见可所写按**表达式**成立 |
| 10 | 不入本域:位算与 `0x` 字面量(随 FFI 波次/位运算档,expr.md 既有)、其他编码族与摘要算法(库域) |
| 11 | 否决 `[]Byte`/`Byte`/`UInt8` 提前:Bytes 是连续块不是表(列表契约是共享与归纳);字节值无第二身份且以算为主,0..255 的 `Int` 即域词(§1 否决表) |

## 1. 身份:平行于 String

主轴一句:**String 是文本,Bytes 是字节;两界只有一道门——UTF-8 编解码。**

| | String | Bytes |
|---|---|---|
| 元素 | Char 标量(char.md) | 0..255 的 Int |
| 承诺 | 恒合法 UTF-8 | 无承诺,任意字节 |
| 取值 | `at` O(n) 明码 | `get` O(1) 明码 |
| 计长 | `length` 数标量 | `length` 数字节 |
| 遍历 | `chars -> []Char` | `to_ints -> []Int` |
| 字面量 | `"…"` | 无——构造走函数 |

- Bytes 是内建复合,不入四复合括号(type.md §3)——同 String 的
  既有位置;`get` 唯一入口、索引语法永禁(access.md 同规)。
- 不可变:装箱/共享/引用计数随 repr.md 一体,零新裁定。
- PascalCase(type.md §6);`<bytes>` 不用——它有结构,不是能力。

### 被否决备选:`Byte`、`[]Byte` 与 `UInt8` 提前

| 方案 | 否决理由 |
|---|---|
| `[]Byte`(字节作列表) | 列表契约是结构共享与归纳模式(`++` 尾共享、`{h, ..t}` O(1) 尾,repr.md §7、pattern.md §3);字节块要的是连续与偏移量。两种契约共用一个名字,则同型异布局、“成本进名字”(access.md 裁定 2)在同型内分叉——最重的一条。且元素级节点使每字节多倍内存与计数(repr.md §4:1MB 文件即百万堆对象),模式 rest 在连续实现上每步 O(n),协议解析 O(n²);Haskell `[Word8]` → packed `ByteString` 是同一实验的历史答案 |
| `Byte` 元素型 | 安全只值 `from_ints` 已有的一个 Option(Bytes 构造即合法);无字面量、无自显示,缺 Char 赚得身份的第二身份(char.md);字节天天被算术(校验和、varint),元素型把 `b1 * 256 + b2` 变成 to_code/from_code 往返。要身份,`Byte: type = Byte UInt` 单变体用户自造即得(type.md §2) |
| String 改 `[]Char` 求对称 | 方向反了:`String ≠ []Char` 是既定先例(连续 UTF-8 + `chars` 桥,char.md);Bytes 是同一设计动作的第二次,不是新增例外 |
| `UInt8` 提前(字节值用机词) | 机词整族随 FFI 波次(expr.md §6),单点提前是渗漏,且须提前裁溢出语义与尺寸转换族——零成员先到等于整族债务先到。字节计算立即越域(校验和、varint、`b1 * 256 + b2`):UInt8 能持不能算,每个计算点逐个 `to_uint`。计算接口恒是 `Int`(Char.to_code 同款);换得的 `from_u8s` 全函数只省常量处一个 `expect`,签名自文档是文档不是安全;`[]U8` 互转本就预约在 FFI 波次(§5) |

## 2. 服务面

| 服务 | 签名 | 语义/成本 |
|---|---|---|
| `Bytes.length` | `(Bytes) -> Int` | 字节数;O(1) |
| `Bytes.get` | `(Bytes, Int) -> Option Int` | 越界 None;O(1) |
| `Bytes.append` | `(Bytes, Bytes) -> Bytes` | 连接;O(n) 全拷 |
| `Bytes.to_ints` | `(Bytes) -> []Int` | 桥;遍历归 List 组合子 |
| `Bytes.from_ints` | `([]Int) -> Option Bytes` | 逐元素 0..255;越界 None(域检查) |
| `Bytes.to_hex` | `(Bytes) -> String` | 小写连续;O(n) |
| `Bytes.from_hex` | `(String) -> Result Bytes ParseError` | 输入验证;空串 → Ok 空 |
| `String.to_bytes` | `(String) -> Bytes` | UTF-8 编码;全函数 |
| `Bytes.to_string` | `(Bytes) -> Result String ParseError` | UTF-8 解码;失效载荷带偏移 |

- `++` 扩域:`b1 ++ b2 ≡ Bytes.append(b1, b2)`(expr.md §1 域表
  增补);优先级与结合随 `++` 既有。
- `==` 结构深相等(逐字节,derive.md §4);`<` 字节字典序——内建
  手工定义(derive.md §5);`sort_by` 可用。
- 空字节串:`"".to_bytes()`——不配 `Bytes.new/empty` 词。

## 3. 编解码:三分法落位

convert.md 三分法在二进制域的落位:

| 性质 | 词 | 失败形 |
|---|---|---|
| 编码(String 恒合法 UTF-8) | `String.to_bytes` | 无——全函数 |
| 解码(外部字节不可信,要为什么) | `Bytes.to_string` | `Err (Invalid …)`,载荷含首个失效偏移与现场 |
| 逐字节构造(问有无) | `Bytes.from_ints` | `None` |
| hex 解析(输入验证) | `Bytes.from_hex` | `Err`,奇长度/非十六进制字符 |

往返性质(convert.md §4 精神):

```
Bytes.to_string(s.to_bytes())  ==  Ok s      // String 恒合法 UTF-8,双向恒成立
Bytes.from_hex(b.to_hex())     ==  Ok b      // hex 往返
```

解码失效例:`"ed a0 80"`(代理对的 UTF-8 形)→ Err,载荷
`"offset 0: ed a0 80"` 一类——消息文案属诊断词汇表域。

## 4. I/O:两道门

```
File.read_bytes:  (File, Int) !-> Result Bytes IoError      // 顺序读至多 n 字节
File.write_bytes: (File, Bytes) !-> Result _ IoError        // 写在当前位置
Fs.read_bytes:    (Fs, String) !-> Result Bytes IoError     // 便利镜像:整文件
Fs.append_bytes:  (Fs, String, Bytes) !-> Result _ IoError  // 便利镜像
```

- **read_bytes 永不 `InvalidData`**:任意字节合法;短读即 EOF,
  `Ok` 短串——EOF 不是错误(B3)一以贯之。
- **`File.read` 保持诚实**:文本门遇非 UTF-8 照旧
  `Err InvalidData`(io-world §3)。以前的 Err 是死路(无正门),
  现在是守卫——想要原始字节,read_bytes 是正门;拒绝与接纳各就各位。
- `with_file` 回调与 `<-` 链照常;OpenMode 不变;`In` 控制台层
  纯文本,不设字节门。

```eko
png_magic: []Int = {137, 80, 78, 71, 13, 10, 26, 10}

is_png: (fs: Fs, path: String) !-> Result Bool IoError =
    with_file(fs, path, Read, (f):
        head <- f.read_bytes(8).try()
        Ok head.to_ints() == png_magic)
```

## 5. 与 FFI 的解绑边界

need.md 的抱怨是一个"捆"字:二进制是 CLI 刚需,不该等 FFI。解绑
的精确边界:

| 面 | 今日开门 | 仍随 FFI 波次 |
|---|---|---|
| 值 | `Bytes` + 全部纯服务 | —— |
| I/O | read/write_bytes | —— |
| 字节值拼写 | 域词 `Int`(0..255) | 机词 `U8/U16/…`(expr.md §6) |
| 位算 | —— | 函数族 + 运算符裁定(expr.md §1 既有推迟) |
| 字面量 | —— | `0x/0b`(位运算档另立) |

域词先行、机词不拖不抢:字节是"0..255 的自然数"(`Char.to_code`
同款),不是"8 位机字"。FFI 到来时 `Bytes` 与 `[]U8` 的零拷贝互转
属彼档,不在本档预支。

## 6. 显示

show 与 inspect 同形——`Bytes.from_hex("…")` 构造子应用形:

```
b := "hi".to_bytes()
"{b}"        // Bytes.from_hex("6869")
inspect(b)   // Bytes.from_hex("6869")
```

构造子应用形先例:`Some 3`(derive.md §3)。所见可所写按**表达式**
成立:inspect 输出即合法程序文本,求值回原值。长字节串的 inspect
全量输出——结构通道不截断,诚实优先;测试断言域内截断是使用者的事。

## 7. 禁止清单

- **字节字面量(`0x…`/`b"…"`/`bytes"…"`)——构造走函数;`0x` 随
  位运算档另立(expr.md §7 既有)。
- **索引语法 `b[i]`**——get 唯一入口(access.md;Bytes 同规)。
- **可变 Bytes/builder/resize 词汇**——值不可变;append O(n) 全拷
  如实记,批构走 `from_ints`。
- **其他编码族入 prelude(UTF-16/latin-1/GBK)**——UTF-8 是唯一
  内建门;编码转换属库域。
- **摘要/crc 入 prelude**——算法域;`get`/`to_ints` 足以供库实现。
- **Bytes 与 String 隐式互转**——唯一门是 to_bytes/to_string,
  显式、带失败语义。

## 8. 与各档接缝

- io-world.md §4/§10:read/write_bytes 增补,置位清单同步。
- char.md §4:字节域注记收口——`s.to_bytes().length()`,
  `byte_length` 不配词。
- expr.md 裁定 11/§1:`++` 域扩字节串。
- derive.md §3/§5:inspect 表与 `<` 内建清单增补。
- convert.md:ParseError 复用于解码与 hex 解析;三分法表落两行。
- repr.md:Bytes 是复合值,装箱/共享/RC 照裁,零新裁定。
- module.md §7:prelude 开域清单入 `Bytes`。

## 9. 速查

```
head <- f.read_bytes(8).try()      // 字节门:永不 InvalidData
case b.to_string():                // 解码:输入验证,Err 带为什么
    Ok s  -> out.println(s)
    Err e -> out.println("非 UTF-8: {e}")
b.to_hex()                         // "6869";from_hex 是逆
s.to_bytes().length()              // String 的字节长;byte_length 不配词
"".to_bytes()                      // 空字节串
b1 ++ b2                           // ≡ Bytes.append;O(n) 全拷
b.get(0)                           // Option Int;O(1)
inspect(b)                         // Bytes.from_hex("6869")
Bytes.from_ints(m).expect("magic") // 无字面量的代价;魔数常量习语,0x 另立
```
