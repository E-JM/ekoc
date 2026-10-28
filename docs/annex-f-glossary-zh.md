# Annex F. Chinese Terminology
## F.1 Purpose
本附录是信息性内容，不构成规范要求。它将 Clause 4 定义的术语、Clause 4.8 列出的符号，以及诊断与恐慌代码族整理为英中对照表，供中文读者查阅。英文术语具有权威性；中文译文只用于理解，术语的规范含义以 Clause 4 为准（Clause 1.4.5）。每行保留英文术语，以便全文检索。代码标识符、`ctype`、`Pure`、`C.In` 以及 E-/W-/P- 代码不翻译。第三列给出该术语的规范归属条款；代码的完整清单见 Annex B。各表按 Clause 4 的分组排列。

## F.2 Terms
### F.2.1 General and Lexical Terms
| English term | 中文 | Clause |
|---|---|---|
| conforming implementation | 符合规范的实现 | Clause 2.1 |
| conforming program | 符合规范的程序 | Clause 2.2 |
| diagnostic | 诊断信息 | Clause 2.4 |
| error | 错误 | Clause 2.4 |
| warning | 警告 | Clause 2.4 |
| implementation-defined | 实现定义 | Clause 2.5 |
| unspecified | 未规定 | Clause 2.5 |
| undefined behavior | 未定义行为 | Clause 2.6 |
| source text | 源文本 | Clause 5.1 |
| compilation unit | 编译单元 | Clause 7.1.1 |
| token | 词法单元 | Clause 5.3.1 |
| identifier | 标识符 | Clause 5.5.1 |
| keyword | 关键字 | Clause 5.6.1 |
| contextual word | 上下文词 | Clause 5.6.2 |
| literal | 字面量 | Clause 5.7, Clause 5.8, Clause 5.9 |
| block | 块 | Clause 6.2.2 |
| member (of a block) | 成员（块内） | Clause 6.2.2 |
| header | 块首行 | Clause 6.2.3 |
| binding | 绑定 | Clause 6.3.1 |
| declaration | 声明 | Clause 6.3 |
| annotation slot | 标注槽位 | Clause 6.3.1 |
| lambda | 匿名函数 | Clause 6.4.1 |
| pattern | 模式 | Clause 6.8.1 |
| scrutinee | 被匹配值 | Clause 6.7.1 |
| arm | 匹配分支 | Clause 6.7.1 |
| guard | 守卫 | Clause 6.7.3 |
| type expression | 类型表达式 | Clause 6.9.1 |
| arrow annotation | 箭头标注 | Clause 6.9.3 |

### F.2.2 Names and Modules
| English term | 中文 | Clause |
|---|---|---|
| module | 模块 | Clause 7.2.1 |
| entry module | 入口模块 | Clause 7.1.3 |
| project root | 项目根目录 | Clause 7.2.2 |
| import | 导入 | Clause 7.2.3 |
| dependency graph | 依赖图 | Clause 7.2.4 |
| interface | 接口 | Clause 7.3.2 |
| public | 公开 | Clause 7.3.1 |
| private | 私有 | Clause 7.3.1 |
| free function | 自由函数 | Clause 7.1.2 |
| receiver | 接收者 | Clause 7.4.3 |
| uniform function call syntax (UFCS) | 统一函数调用语法 | Clause 6.5.2 |
| type namespace | 类型命名空间 | Clause 7.4.5 |
| member (of a type namespace) | 成员（类型命名空间） | Clause 7.4.3 |
| resolution station | 解析站点 | Clause 7.4.3 |

### F.2.3 Types
| English term | 中文 | Clause |
|---|---|---|
| type | 类型 | Clause 7.5.1 |
| value | 值 | Clause 8.5 |
| nominal type | 名义类型 | Clause 7.5.2 |
| anonymous type | 匿名类型 | Clause 7.5.3 |
| composite type | 复合类型 | Clause 7.5.3 |
| record | 记录 | Clause 7.5.3 |
| tuple | 元组 | Clause 7.5.3 |
| list | 列表 | Clause 7.5.3 |
| dictionary | 字典 | Clause 7.5.3 |
| algebraic data type (ADT) | 代数数据类型 | Clause 7.5.2 |
| variant | 变体 | Clause 7.5.2 |
| constructor | 构造子 | Clause 7.5.2 |
| payload | 载荷 | Clause 7.5.2 |
| single-variant type | 单变体类型 | Clause 7.5.2 |
| type variable | 类型变量 | Clause 7.6.1 |
| binder | 绑定器 | Clause 7.6.2 |
| generic declaration | 泛型声明 | Clause 7.6.1 |
| generalization | 泛化 | Clause 7.6.3 |
| instantiation | 实例化 | Clause 7.6.4 |
| monomorphization | 单态化 | Clause 8.7.1 |
| same shape | 同形状 | Clause 7.5.5 |
| shape adaptation | 形状适配 | Clause 7.5.5 |
| bottom type | 底类型 | Clause 7.5.4 |
| unit type | 单位类型 | Clause 7.5.4 |
| exhaustiveness | 穷尽性 | Clause 7.7.2 |

### F.2.4 Capabilities and Failure
| English term | 中文 | Clause |
|---|---|---|
| world | 世界 | Clause 7.9.1 |
| world contact | 世界接触 | Clause 7.9.2 |
| capability | 能力 | Clause 7.9.1 |
| birth-granted capability | 初始授予能力 | Clause 7.9.1 |
| opt-in capability | 选择性能力 | Clause 7.9.1 |
| derived capability | 派生能力 | Clause 9.12.5 |
| capability module | 能力模块 | Clause 9.1.1 |
| host | 宿主 | Clause 2.3 |
| world bit | 世界位 | Clause 7.9.2 |
| world-touching | 接触世界 | Clause 7.9.3 |
| pure | 纯 | Clause 7.9.3 |
| open bit | 开放位 | Clause 7.9.6 |
| dispatcher | 分派器 | Clause 7.9.6 |
| bit adaptation | 位适配 | Clause 7.9.5 |
| handle | 句柄 | Clause 9.12.3 |
| generation | 代次 | Clause 9.12.3 |
| failure taxonomy | 失败分类法 | Clause 9.2 |
| panic | 恐慌 | Clause 8.4.2 |
| abort | 中止 | Clause 8.4.3 |

### F.2.5 Evaluation, Runtime, and Library
| English term | 中文 | Clause |
|---|---|---|
| strict evaluation | 严格求值 | Clause 8.1.1 |
| heap object | 堆对象 | Clause 8.5.2 |
| scalar | 标量 | Clause 8.5.1 |
| capture | 捕获 | Clause 8.2.3 |
| closure | 闭包 | Clause 8.2.3 |
| acyclicity | 无环性 | Clause 8.6.1 |
| reference counting | 引用计数 | Clause 8.6.2 |
| tail position | 尾位置 | Clause 8.8.2 |
| self tail call | 自尾调用 | Clause 8.8.3 |
| task | 任务 | Clause 8.9.1 |
| scope | 作用域 | Clause 8.9.1 |
| cancellation | 取消 | Clause 8.9.2 |
| standard library | 标准库 | Clause 9.1.1 |
| std | std 模块 | Clause 9.1.1 |
| companion module | 伴随模块 | Clause 9.1.3 |
| derived service | 派生服务 | Clause 9.8.1 |
| display channel | 显示通道 | Clause 9.8.1 |
| structural channel | 结构通道 | Clause 9.8.1 |
| value layer | 值层 | Clause 9.8.4 |
| key domain | 键域 | Clause 9.10.4 |
| test | 测试 | Clause 9.14.1 |
| test root | 测试根 | Clause 9.14.6 |
| extern declaration | extern 声明 | Clause 10.2.1 |
| Pure | Pure 保证 | Clause 10.2.2 |
| ctype | ctype 布局记录 | Clause 10.3.3 |
| C.In | C.In 只读借用 | Clause 10.3.4 |
| marshalling whitelist | 编组白名单 | Clause 10.3.5 |
| veneer | 嵌入薄层 | Clause 10.7.1 |
| reserved symbol | 保留符号 | Clause 10.6 |

### F.2.6 Additional Technical Terms
| English term | 中文 | Clause |
|---|---|---|
| layout / indentation level | 布局 / 缩进层级 | Clause 5.2, Clause 5.2.1 |
| trailing callback / inverted binding | 尾随回调 / 反向绑定 | Clause 6.2.5, Clause 6.3.4 |
| wildcard / or-pattern | 通配符 / 或模式 | Clause 6.8.1, Clause 6.8.3 |
| expected type | 期望类型 | Clause 6.6.2 |
| attenuation | 衰减 | Clause 9.12.5 |
| trusted computing base | 可信计算基 | Clause 10.1.4 |
| shim | C 垫片 | Clause 10.3.6 |

## F.3 Symbols
| Symbol | 含义 | Clause |
|---|---|---|
| `_` | 单位类型、单位值、通配符、丢弃或私有名前缀，依位置而定 | Clause 7.5.4, Clause 7.3.1 |
| `!` | 箭头上的接触世界标记 | Clause 7.9.3 |
| `?` | 箭头上的开放位标记 | Clause 7.9.3 |
| `->` | 纯函数箭头 | Clause 7.9.3 |
| `!->` | 接触世界的函数箭头 | Clause 7.9.3 |
| `?->` | 开放函数箭头 | Clause 7.9.3 |
| `=` | 初始绑定 | Clause 6.3.2 |
| `:=` | 重绑定 | Clause 6.3.3 |
| `<-` | 反向绑定 | Clause 6.3.4 |
| `->`（匹配分支中） | 分支箭头，分隔模式与结果 | Clause 6.7.1 |
| `\|` | 变体分隔符；或模式分隔符（依位置而定） | Clause 6.8.3, Clause 6.9.5 |
| `++` | 连接或字典合并 | Clause 6.5.4, Clause 7.8.6 |
| `..` | 范围构造，右端开区间 | Clause 6.5.6 |
| `..=` | 范围构造，右端闭区间 | Clause 6.5.6 |

## F.4 Diagnostics
诊断代码由 Annex B 统一规定；本表只给出中文释义，代码本身不翻译。E- 为编译期错误，W- 为警告，P- 为运行时恐慌代码。严重级别见 Clause 2.4；恐慌的中止语义见 Clause 8.4.2。代码一经发布即不复用、不重定义。

### F.4.1 Error Codes
| Code | 含义 | Clause |
|---|---|---|
| `E-ctype-field` | `ctype` 字段不是机器字或嵌套 `ctype` | Clause 10.3.3 |
| `E-cycle-import` | 循环导入 | Clause 7.2.4 |
| `E-empty-slot` | 插值槽为空 | Clause 5.9.3 |
| `E-exhaust` | 穷尽性匹配缺少分支 | Clause 7.7.2 |
| `E-extern-type` | extern 签名中的类型不在编组白名单内，或含类型变量/可变参数 | Clause 10.2.3, Clause 10.3.5 |
| `E-instantiation-depth` | 泛型实例化不终止 | Clause 8.7.3 |
| `E-json-shape` | 类型没有默认 JSON 同构形式 | Clause 9.9.3 |
| `E-literal-overflow` | 数值字面量超出目标类型范围，或浮点字面量超出有限范围 | Clause 5.7.1, Clause 5.7.2 |
| `E-name-case` | 命名大小写违规 | Clause 5.5.2 |
| `E-name-conflict` | 同层名称冲突 | Clause 7.4.6 |
| `E-namespace-owner` | 在类型所属模块之外声明其命名空间成员 | Clause 7.4.5 |
| `E-no-eq` | 类型没有 `eq`，例如载荷含能力或函数 | Clause 9.8.4 |
| `E-nominal-mismatch` | 名义类型之间不能直接流转 | Clause 7.5.2 |
| `E-open-bit-position` | `?` 出现在无法泛化的位置 | Clause 7.9.6 |
| `E-open-bit-unfed` | 开放位没有参数供给 | Clause 7.9.6 |
| `E-phantom` | 幻影参数或仅出现在结果中的变量 | Clause 7.6.2, Clause 7.6.5 |
| `E-refutable-binding` | 值绑定位置出现可驳模式 | Clause 7.7.1 |
| `E-reserved-symbol` | extern 声明引用保留符号 | Clause 10.6 |
| `E-shape-mismatch` | 形状不同，无法适配 | Clause 7.5.5 |
| `E-tab` | 以制表符作为空白 | Clause 5.2.3 |
| `E-unbound-name` | 名称在任何可见作用域中都没有声明 | Clause 7.4.2, Clause 7.4.6, Clause 9.1.2 |
| `E-world-bit` | 世界位不一致，多写或少写 `!` | Clause 7.9.2 |

### F.4.2 Warning Codes
| Code | 含义 | Clause |
|---|---|---|
| `W-deprecated` | 使用已弃用成员 | Clause 11.5 |
| `W-redundant-import` | 显式 `import std` 冗余 | Clause 9.1.2 |
| `W-unreachable` | `Never` 调用之后的代码不可达 | Clause 8.4.4 |
| `W-unused` | 未使用的绑定或导入 | Clause 9.14.6 |

### F.4.3 Panic Codes
| Code | 含义 | Clause |
|---|---|---|
| `P-panic` | `panic(msg)` 或标准库不变量违例 | Clause 8.4.2 |
| `P-unwrap` | 对 `None` 或 `Err` 调用 `unwrap` | Clause 9.2.3 |
| `P-expect` | 对 `None` 或 `Err` 调用 `expect` | Clause 9.2.3 |
| `P-div` | 除以零 | Clause 7.8.4 |
| `P-overflow` | 算术溢出 | Clause 7.8.4 |

## F.5 Translation Notes
- 英文术语与条款正文具有权威性；中文译文是阅读辅助，不改变 Clause 4 的定义。`capability` 译“能力”，不译“权限”；`world` 译“世界”，`world bit` 译“世界位”，不译“效果位”。
- `handle` 译“句柄”，`generation` 译“代次”而非“世代”，`dispatcher` 译“分派器”，`veneer` 译“嵌入薄层”，`scrutinee` 译“被匹配值”；`panic` 译“恐慌”但代码标识符保持原样，`abort` 译“中止”。
- `nominal type` 采用“名义类型”（备选“标称类型”）；`constructor` 采用“构造子”（备选“构造器”）；`monomorphization` 采用“单态化”（备选“单形化”）；`token` 采用“词法单元”（备选“记号”）。`member` 在 Clause 4.2 与 Clause 4.3 中各有一个定义，本表以“成员（块内）”和“成员（类型命名空间）”区分。
- 代码标识符、`ctype`、`Pure`、`C.In`、`std` 以及 E-/W-/P- 代码不翻译。诊断消息本身为英文（Annex B）；本表的中文释义不是诊断输出的一部分。中文行文在英文术语与代码两侧留空格。
