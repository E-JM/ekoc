# eko 诊断 —— 词汇表、码与警告清单

状态:设计定案(2026-10)。结论先行,理由随文。
本文关闭 need.md 小规范洞"诊断词汇表:消息语言/错误码/警告清单"。
三条裁定:消息恒英文;错误码 = 稳定符号码;警告清单封闭。诊断 = 编译期
消息;运行时 panic 输出属宿主域(B4),不入本档。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 诊断消息恒英文;档案英文化是既定方向(另批迁移,不在本档) |
| 2 | 错误码 = 稳定符号码:`E-` + kebab 词;警告 `W-` 前缀;码可 grep |
| 3 | 码一经发布不可复用、不可改义;消息措辞可改,码与语义冻结 |
| 4 | 警告清单封闭:`W-unreachable`、`W-unused` 两员;新警告须修档 |
| 5 | 警告不可压制(无属性/pragma/注释开关);消警 = 修码——未用绑定改名 `_x` 起头 |
| 6 | W-unused 只及绑定与 import,**参数不参与**——回调签名常带多余位(`(p, i):` 语料) |
| 7 | 诊断结构:`码: 一行陈述` + 缩进细节行(hint);位置格式属实现域 |

## 1. 语言:英文

- 工具链惯例 + 生态亲和:错误文本可搜索、可引用;与档案双语分轨——
  档案英文化是既定方向(2026-10 记录),迁移另批。
- 文体:小写开头、无句号、陈述事实不给判词;修复建议入 hint 行;
  涉及的名字以反引号包围。细则随词汇表逐词条冻结。

## 2. 码:稳定符号码

形:`E-nominal-mismatch` 一类(`E-` + kebab 语义词)。首批词条:

| 码 | 语义 | 出处 |
|---|---|---|
| `E-nominal-mismatch` | 名义对名义不相通 | type.md §7.3 既有例 |
| `E-shape-mismatch` | 形不适配(非同形) | type.md §7.2 |
| `E-exhaust` | 穷尽性缺臂 | branch.md 裁定 3 |
| `E-world-bit` | 位对账失败(过声明/欠声明) | io-world B2 |
| `E-open-bit-unfed` | 开位无人喂 | io-world §5.2 |
| `E-phantom` | 幻影参数 / result-only 变量 | generics.md §2.1/§3.2 |
| `E-name-case` | 命名大小写违规 | type.md §6 |
| `E-name-conflict` | 名字冲突(同层/构造器撞) | module.md 裁定 10 |
| `E-cycle-import` | 循环依赖 | module.md 裁定 7 |
| `E-no-eq` | 无 `==` 的型用于相等/键/断言 | derive.md §4 |
| `E-json-shape` | 无默认 JSON 同构(非串键字典等) | json.md |
| `E-instantiation-depth` | 泛型实例化展开不终止 | repr.md §6 |

- 词汇表开放增长,入表即冻结;本表是首批。
- 为什么不是数字码:名可 grep(全档主轴);数字需查表,引用诊断只能
  引码不能引义。
- 为什么不是无码:消息措辞会改,码是稳定锚——文档、issue、测试的
  引用单位。

## 3. 警告清单(封闭)

| 码 | 语义 | 出处 |
|---|---|---|
| `W-unreachable` | Never 调用后的代码 | never.md §4 |
| `W-unused` | 未用绑定 / 未用 import | test.md §6(按全部编译根合计) |

- **封闭理由**:警告清单是契约——用户据此做"零警告"工程;开缺口
  即破坏契约。新警告 = 修档裁定,不作实现自由。
- **不可压制**:无 `#[allow]`/pragma(test.md §7 属性标注禁令同精神);
  消警即修码:
  - `W-unused` → 改名 `_x` 起头(局部 `_` 家族的"不在乎"义,
    module.md §5)或删除;
  - `W-unreachable` → 删除或重构控制流。
- 参数不参与 W-unused(裁定 6):回调按位收参,多余位是常态
  (`each_with_index((p, i): …)` 既有语料)。

## 4. 结构与文体

```
E-nominal-mismatch: `Person` does not flow into `Human` (both nominal)
  hint: convert via a shared anonymous shape, or rebuild field by field
```

- 一行消息(码 + 陈述)+ 缩进细节(hint、涉及名字);位置
  (`文件:行:列`)属实现域。
- 诊断三级(错误/警告/无)既有;位域恒为错误(io-world B2)重申;
  panic 消息是运行时输出,不属词汇表。

## 5. 与各档接缝

- never.md §4、test.md §6:两警告的既有出处。
- module.md §5:局部 `_x` 语义精化——"不参与 W-unused"。
- 诊断措辞与档案同源:一条裁定一个词条,词汇表随档增长。

## 6. 禁止清单

- **数字错误码**——查表负担,不可 grep。
- **可压制警告(allow/pragma/注释开关)**——警告清单是契约,压制是谎言。
- **警告清单开放增长**——新警告走修档,契约稳定。
- **运行时 panic 消息入词汇表**——panic 是宿主域输出。

## 7. 速查

```
E-nominal-mismatch        // 名义不通:码即词汇,可 grep
W-unused                  // 未用绑定/import;_x 起头即消
W-unreachable             // Never 后代码;删或重构
码冻结,措辞可改            // 引用引码,不引消息
```

## 修订:运行时 panic 码族(09-library.md §7.6)

运行时 panic 的 CLI 输出升格为规范面(原"panic 属宿主域"句随改):
首行语法 `panic [P-code]: message`;`P-` 码族与 `E-`/`W-` 同构、同
冻结规则;回溯恒印,帧行 `  at module.function (file:line:column)`,
自尾消除帧不现;退出码 1;abort 类(栈溢出/内存耗尽)非 panic,
`abort: reason` 行+平台中止退出。首批 P- 码:P-panic/P-unwrap/
P-expect/P-assert/P-assert-eq/P-div/P-overflow。测试 runner 依此
语法捕获(09 §6.6 接缝)。
