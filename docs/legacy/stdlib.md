# eko 标准库补面 —— Fs、格式化、数学、文本与集合

状态:设计定案(2026-10)。结论先行,理由随文。
本文关闭 need.md 可用性洞四项:Fs 补面、格式化、数学库、文本/集合补面
(含 Set、Dict 键域、排序稳定性)。原则一条:**补面不补语法**——全部是
类型命名空间里的函数与既有机器的组合,零新记号。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | Fs 补面:`list_dir/mkdir/mkdir_all/rm/rm_dir/rename/stat`,全置位;stat 产 `FileInfo [size: UInt, mtime: Time, is_dir: Bool]` |
| 2 | `list_dir` 产名字表(String 字典序,确定性);拼路径走插值,路径库另立若痛 |
| 3 | 格式化 = 函数族,无格式 DSL:`Int.to_hex/to_oct/to_bin` + `parse_hex` 族、`Float.to_fixed/to_exp`、`String.pad_left/pad_right`;插值与 show 不动(仅最短往返) |
| 4 | Float 数学域外取 **IEEE 值**:`sqrt(-1) = NaN`、`log(0) = -Inf`;NaN 是值,`is_nan/is_inf/is_finite` 谓词陪跑 |
| 5 | 文本补面:contains/starts_with/ends_with/replace(全替换)/slice(Option,标量域)/trim 族;Unicode 大小写映射、正则属库域 |
| 6 | 集合补面:`List.take/drop`(截断语义,全函数)、`take_while/drop_while`(`?` 分派者)、`join`;`sort_by` **稳定**,O(n log n);`List.sort` 默认序走 `<` |
| 7 | `Set t` 入 prelude:**无记号、无字面量**——`Set.of` 构造;四复合不动(type.md §3.2 注记就此落定) |
| 8 | Dict 键域 = Set 元素域 = **有 `==` 的数据型**(capability 与函数除外);结构相等即键相等;NaN 键恒 miss,如实记 |
| 9 | `Set` 保插入序(与 Dict 同裁,derive 裁定 10);dedup 保首见;union/intersect/diff 各一 |

## 1. Fs 补面

| 服务 | 签名 | 语义 |
|---|---|---|
| `Fs.list_dir` | `(Fs, String) !-> Result []String IoError` | 名字表,字典序;非目录 → Err NotFile;不含 `.`/`..` |
| `Fs.mkdir` | `(Fs, String) !-> Result _ IoError` | 单级;已存在 → Err Exists |
| `Fs.mkdir_all` | `(Fs, String) !-> Result _ IoError` | 递归建链;已存在且为目录 → Ok |
| `Fs.rm` | `(Fs, String) !-> Result _ IoError` | 删文件;目录 → Err NotFile |
| `Fs.rm_dir` | `(Fs, String) !-> Result _ IoError` | 删空目录;非空/非目录 → Err (Other …) |
| `Fs.rename` | `(Fs, String, String) !-> Result _ IoError` | 改名/移动;跨设备等 → Err (Other …) |
| `Fs.stat` | `(Fs, String) !-> Result FileInfo IoError` | 见下;符号链接语义属宿主域,如实记 |

```
FileInfo: type = [size: UInt, mtime: Time, is_dir: Bool]
```

- 错误映射沿用**封闭 IoError**(io-world §3):目录非空、非目录删、
  跨设备一类细节失败走 `Other String`——"变体一次配齐"的既有裁定不破。
- `list_dir` 产**名字**不产路径:拼路径走插值 `"{dir}/{name}"`
  (分隔符随宿主);路径库(absolute/join/normalize)另立若痛。
- size 对目录是平台定义值,如实记;mtime 是壁钟点(time.md),show
  即 ISO 形。
- 衰减组合子(`Fs.root/read_only`)对补面同样生效——授权层不动。

## 2. 格式化:函数族,无 DSL

主轴一句:**show 管显示,函数管格式;格式串语言永禁。**

| 服务 | 签名 | 语义 |
|---|---|---|
| `Int.to_hex/to_oct/to_bin` | `(Int) -> String` | 正数最短小写;负数为 64 位补码形;UInt 族同构无符号 |
| `Int.parse_hex/parse_oct/parse_bin` | `(String) -> Result Int ParseError` | 输入验证;无前缀、接受 ±、大小写不敏感;UInt 族同构 |
| `Float.to_fixed` | `(Float, UInt) -> String` | 定点 n 位;十进制最近舍入,平局远离零;NaN/Inf 如实 |
| `Float.to_exp` | `(Float, UInt) -> String` | 科学计数 n 位尾数;同舍入 |
| `String.pad_left/pad_right` | `(String, UInt, Char) -> String` | 只补不截;已足宽即原样 |

- 插值 `"{e}"` 与 show 不动:仍是最短往返;格式化是显式调用的补面,
  不是第二插值通道。
- `0x` 字面量仍随位运算档(expr.md §7 既有)——`to_hex` 是函数,
  不预支词法;定宽版(FFI 调试)随彼档另立。

## 3. 数学:IEEE 值

主轴一句:**域外是值,不是错误——NaN/Inf 按硬件,谓词陪跑。**

| 服务 | 签名 | 语义 |
|---|---|---|
| `Float.sqrt/exp/log/log2/log10` | `(Float) -> Float` | sqrt(-1)=NaN;log(0)=-Inf;log(-1)=NaN |
| `Float.sin/cos/tan/asin/acos/atan` | `(Float) -> Float` | 弧度制;asin(2)=NaN |
| `Float.atan2` | `(Float, Float) -> Float` | 象限正确 |
| `Float.pow` | `(Float, Float) -> Float` | IEEE:pow(0,0)=1、pow(1,Inf)=1,如实 |
| `Float.is_nan/is_inf/is_finite` | `(Float) -> Bool` | 谓词;is_inf 不分正负 |
| `Float.abs/floor/ceil/trunc/round` | `(Float) -> Float` | trunc 向零(与 to_int 同基);round 半偶 |
| `Int.abs` | `(Int) -> Int` | abs(Int.min) panic(溢出教义,expr.md §2) |

- 与 Float `==` 的 IEEE 裁定(derive.md §4)同轴:NaN 是值,由谓词
  分辨,不由类型分层;出域受检的闸已在转换边界(Float.to_int 对
  NaN/Inf 出 None,convert.md)——值域内自由,出域受检,各司其职。
- libm 跨平台位差如实记:实现应文档化最大误差;确定性以同平台为准。
- bignum、复数、矩阵属库域。

## 4. 文本补面

| 服务 | 签名 | 语义 |
|---|---|---|
| `String.contains` | `(String, String) -> Bool` | 子串有无;空串恒 True |
| `String.starts_with/ends_with` | `(String, String) -> Bool` | 前后缀 |
| `String.replace` | `(String, String, String) -> String` | **全替换**;首次替换另立若痛 |
| `String.slice` | `(String, Int, Int) -> Option String` | 起始+长度,标量域,越界 None;O(n) 明码 |
| `String.trim/trim_start/trim_end` | `(String) -> String` | Unicode White_Space |
| `List.slice` | `([]t, Int, Int) -> Option []t` | 起始+长度;越界 None |

- slice 是域检查(有无)→ Option,与 get 同判(access.md);禁令是
  索引语法,slice 是函数,不受影响。
- 大小写映射(to_upper/to_lower)是 Unicode 全表加语境(土耳其语 i)
  ——属库域;ASCII 版用户自写三行。正则同属库域。

## 5. 集合补面:take 族、join、Set、键域、稳定排序

### 5.1 List 补面

| 服务 | 签名 | 语义 |
|---|---|---|
| `List.take/drop` | `([]t, Int) -> []t` | 截断语义,全函数:n 越界全取/全弃,n < 0 按零 |
| `List.take_while/drop_while` | `([]t, (t) ?-> Bool) ?-> []t` | 谓词分派者(filter 同款) |
| `List.join` | `([]String, String) -> String` | 串接,间隔一参;泛型 show 版另立若痛 |
| `List.sort` | `([]t) -> []t` | 默认序走派生 `<`;记录无 `<`,实例化处报错(与 == 同判) |
| `List.sort_by` | `([]t, (t, t) ?-> Bool) ?-> []t` | 比较子显式(小于);**稳定** |

- **稳定排序是规范级承诺**:相等元素的相对次序不变;量级 O(n log n)。
- 比较子签名 `(t, t) -> Bool`;回调可涉世(`?` 透传,filter 同款)。

### 5.2 Set:无记号

```
Set.of:       ([]t) -> Set t
Set.to_list:  (Set t) -> []t                 // 插入序(与 Dict 同裁)
Set.contains: (Set t, t) -> Bool
Set.insert:   (Set t, t) -> Set t            // 已在即原集
Set.remove:   (Set t, t) -> Set t            // 不在即原集
Set.length:   (Set t) -> Int
Set.union:    (Set t, Set t) -> Set t        // 左序在前,右集新元素按右序
Set.intersect:(Set t, Set t) -> Set t        // 保左序
Set.diff:     (Set t, Set t) -> Set t        // 保左序
```

- **无记号、无字面量**:四复合不动;构造走 `Set.of({1, 2, 3})`,
  空集 `Set.of({})` 配期望类型。`Set.empty: () -> Set t` 不可表达
  (result-only 泛型禁令,generics.md §3.2)——不设词。
- `Set.of` dedup 保首见;序即插入序,确定性可复现。
- Set 是名义 prelude 泛型型(`Set t` 并置应用);`==` 派生,无 `<`
  (容器无序语义,derive.md §5 记录同判)。

### 5.3 Dict 键域

- 键域 = **有 `==` 的数据型**(derive.md §4):capability 与函数除外。
- 结构相等即键相等:列表、记录、元组、ADT、Bytes、Set、字典本身皆可
  作键——type.md §3.1 的 `[[]String]Int` 由此落定。
- Float 键如实:NaN != NaN ⟹ NaN 键恒 miss;0.0 与 -0.0 相等即同键
  (IEEE 教义延伸,如实记)。
- Set 元素域同此(裁定 8)。

## 6. 与各档接缝

- io-world.md §10:Fs 补面入置位清单;封闭 IoError 不增变体。
- access.md:get 唯一入口不动;slice 是域检查函数,同精神。
- type.md §3.2"集合暂无记号"注记落定:入类型、不开记号。
- generics.md §3.2:`Set.empty` 是 result-only 禁词的活例。
- derive.md:稳定排序承诺、`<` 能力面;json.md:to_json 派生。
- convert.md:parse_hex 族入 parse 家族;to_fixed/to_exp 是全函数
  (格式化不是转换失败)。

## 7. 禁止清单

- **格式 DSL(printf/format 串)**——插值 + 具名函数已覆盖;名可 grep。
- **Set 记号/字面量**——四复合不动;`{a, b}` 已让给列表/元组。
- **Unicode 大小写映射、正则入 prelude**——库域;诚实大于便利。
- **路径库入本档**——名字表 + 插值够今日;join/normalize 另立若痛。
- **bignum/复数/矩阵**——库域。
- **不稳定排序选项**——稳定是唯一承诺;快排一类是实现内部自由,
  不可观察。

## 8. 速查

```
fs.list_dir("src")                    // 名字表,字典序
"{dir}/{name}"                        // 拼路径:插值,分隔符随宿主
fs.stat("a.png").try()                // FileInfo [size, mtime, is_dir]
255.to_hex()                          // "ff";-1 → "ffffffffffffffff"
Float.sqrt(-1.0).is_nan()             // True——域外是值
"hi wd".contains("wd")                // True
xs.take_while((n): n < 3)             // ? 分派者
xs.sort_by((a, b): a.age < b.age)     // 稳定;O(n log n)
Set.of({1, 2, 3}).contains(2)         // True;无记号
grid: [[]String]Int = {}              // 列表作键,键域落定
```
