# eko JSON —— 派生同构与往返保真

状态:设计定案(2026-10)。结论先行,理由随文。
本文关闭 need.md 可用性洞"JSON/序列化:derive 派生同构,CLI 刚需"。
主轴四句:`Json` 值型入 prelude;编解码走 ParseError 机器;数据型默认
派生 `to_json/from_json`(覆写机器同 show);ADT 映射取**外标形**。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | `Json` 入 prelude:七变体 `Null / Bool Bool / Int Int / Float Float / Str String / Arr []Json / Obj ([String]Json)` |
| 2 | Int 与 Float 分立:42 进出仍是 Int——往返保真优先于 JSON 单一数字型的表面统一 |
| 3 | `Json.parse: (String) -> Result Json ParseError`;`Json.show` 是 prelude 覆写(derive.md §2)——插值直接产最小形 JSON 文本;`parse(show(j)) == Ok j` |
| 4 | 数据型默认派生 `to_json: (t) -> Json`、`from_json: (Json) -> Result t ParseError`;声明模块可覆写,所有权防孤儿(show 同款) |
| 5 | ADT 映射 = **外标形**:零载荷 → `"Man"`;单载荷 → `{"Ok": 3}`;多载荷(元组)→ `{"Add": [1, 2]}` |
| 6 | 同构表(§3):记录↔Obj、列表/元组/Set↔Arr、String/Char↔Str、Bool↔true/false、Unit↔null、Bytes↔hex Str(往返保真) |
| 7 | 往返保真(规范级):`from_json(to_json(v)) == Ok v`;字段缺余/型不匹配/未知变体 → Err |
| 8 | 非串键字典不入默认同构(编译错误);to_json/from_json 入泛型变量能力面(单态化落地,与 == 同判) |

## 1. 值型

```
Json: type =
    | Null
    | Bool Bool
    | Int Int
    | Float Float
    | Str String
    | Arr []Json
    | Obj ([String]Json)
```

- `Int Int` 载荷形与 `Time 0` 同款(构造子应用即 inspect 形,
  type.md §2 单变体先例)。
- Obj 是串键字典:保插入序(derive 裁定 10)——show 随之,JSON 文本
  序可复现,配置 diff 友好。
- 值构造零魔法:构造子并置 check-only,字面量经期望类型落位
  (`Obj {"a": Int 1}`,generics.md §4.2)。

## 2. 编解码

```
Json.parse: (String) -> Result Json ParseError     // 输入验证;载荷带偏移与现场
Json.show:  (Json) -> String                       // 最小形:无空白
```

- `parse` 是 convert.md parse 族成员:ParseError 复用,三分法落位
  (输入验证,带为什么)。
- `Json.show` 走覆写机器(声明模块即 prelude):插值 `"{j}"` 直接产
  JSON 文本;结构通道 `inspect(j)` 恒构造形,双通道各安其位
  (derive.md §1)。
- 数字文本:Float 走最短往返——`3.5` 出 `3.5`、`3.0` 出 `3.0`;
  `3` 出 `3` 且还原为 `Int 3`,类型决定拼写。
- 字符串转义按 JSON 规范(`\uXXXX`);代理对成对解析,孤立代理拒绝
  ——String 恒合法 UTF-8,入口把关(char.md)。
- 美化打印(indent)属库域。

## 3. 派生:同构表

| eko 型 | JSON 形 | 例 |
|---|---|---|
| 记录 | Obj(字段名即键) | `{name: "wd"}` → `{"name": "wd"}` |
| 列表 / 元组 / Set | Arr(插入序) | `{1, 2}` → `[1, 2]` |
| String / Char | Str | `'a'` → `"a"` |
| Int / Float | 数值 | `42` → `42`;`3.5` → `3.5` |
| Bool | `true` / `false` | 内建映射(derive.md §5 内建手工同判) |
| Unit | `null` | `Result _ IoError` → `{"Ok": null}` |
| Bytes | Str(hex 小写) | `"hi".to_bytes()` → `"6869"` |
| 字典(串键) | Obj | `{"a": 1}` → `{"a": 1}` |
| ADT | 外标形(§4) | `Man` → `"Man"`;`Ok 3` → `{"Ok": 3}` |
| 非串键字典 | ✗ 编译错误 | 显式转换 |
| capability / 函数 | ✗ 编译错误 | 无 `==` 之物不入同构 |

- Bool 特判:true/false 是 JSON 布尔;用户自造枚举(`Yes | No`)照走
  外标形字符串——内建与声明型分轨,与 derive.md §5 内建手工同判。
- Bytes ↔ hex 小写串(bytes.md §2 同基):字段类型已知,解码无歧义;
  往返保真。
- 覆写优先于同构:`Money.to_json` 一经声明即遮蔽默认(§5)。

## 4. 外标形(ADT 映射)

```
Sex:    type = Man | Woman            Man → "Man"
Result: type t e = Ok t | Err e       Ok 3 → {"Ok": 3};Err "x" → {"Err": "x"}
Tree:   type t = Leaf t | Node [Tree t, Tree t, t]
        Node {Leaf 1, Leaf 2, 3} → {"Node": [{"Leaf": 1}, {"Leaf": 2}, 3]}
```

- 零载荷变体 → 字符串;带载荷 → 单键对象:单载荷直嵌,多载荷
  (元组载荷)键下数组。
- serde externally-tagged 同构;CLI 配置最简——零载荷枚举直读直写。
- `from_json`:未知变体/键 → Err(`Invalid "unknown variant …"` 载荷);
  载荷型不匹配 → Err;构造器域受检(错拼即编译错误,module.md §3)。

## 5. 覆写与泛型

- **覆写**:声明模块在类型命名空间写
  `Money.to_json: (Money) -> Json = …` 即遮蔽默认;`from_json` 同权。
  所有权归声明模块,无孤儿覆写——derive.md §2 show 的同一台机器。
- **泛型**:`to_json`/`from_json` 入变量能力面(generics.md §3.4
  增补):泛型函数内对 `t` 直接调用,实例化处落具体派生或覆写;
  capability/函数/非串键字典实例化处报错——与 `==` 同判。
- 编译期泛型分派(type.md §5):零运行时反射。

```eko
load_config: (s: String) -> Result Config String =
    j <- Json.parse(s).map_err((e): "config: {e}").try()
    Config.from_json(j).map_err((e): "config: {e}")
```

## 6. 与各档接缝

- derive.md:派生家族添 to_json/from_json(裁定 12);覆写与双通道
  机器不变。
- convert.md:ParseError 复用;Json.parse 入 parse 族。
- generics.md §3.4:能力面表增两行。
- module.md §7:Json 入 prelude 清单。
- bytes.md:Bytes↔hex 与 from_hex/to_hex 同基(小写连续)。

## 7. 禁止清单

- **格式串 DSL / 路径查询(jq 一类)**——库域。
- **默认美化打印**——show 最小形;indent 属库域。
- **非串键字典静默同构**——编译错误,显式转换;不静默扭曲。
- **YAML/TOML 入 prelude**——库域;JSON 是唯一内建序列化面。
- **运行时反射**——派生是编译期展开,零反射面。

## 8. 速查

```
j <- Json.parse(s).try()              // Result Json ParseError
"{person.to_json()}"                  // 插值即 JSON 文本(show 覆写)
inspect(j)                            // Obj {"a": Int 1}——结构通道
Man → "Man";Ok 3 → {"Ok": 3}          // 外标形
"hi".to_bytes() → "6869"              // Bytes:hex 往返保真
from_json(to_json(v)) == Ok v         // 规范级往返
```
