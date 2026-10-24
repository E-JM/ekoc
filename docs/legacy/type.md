# eko 类型声明 —— `type` 槽位

状态:语法定案(2026-10)。结论先行,理由随文。
2026-10 增订:递归型开门(§8);值表示与内存管理定案于 repr.md。
2026-10 增订:集合注记落定——`Set` 入 prelude 无记号(stdlib.md §5.2)。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 类型声明是绑定,`type` 落在注解槽:`X: type = RHS` |
| 2 | RHS 是声明形:`\|` 代数型、`[...]` 记录型 |
| 3 | 泛型参数列于 `type` 之后,应用时并置 |
| 4 | 类型的全部身份在编译期完成;运行时里生活的只有值 |
| 5 | 声明即名义:同形异名互不相通,转换显式;无别名 |
| 6 | 匿名复合形结构相等;**形适配单向**:名义值流入同形匿名形(实参/操作数/期望位),匿名永不冒充名义 |
| 7 | 同形非宽度;无自动合取(左定型右适配);形适配不进容器与函数类型内部 |
| 8 | 递归型开门:RHS 可自引用、模块内互引用;递归位恒指针,零新增检查(表示见 repr.md) |

## 1. 槽位

`type` 是注解槽上的词。类型声明与其余绑定共用同一文法形状,行首是名字:

```eko
Sex:     type = Man | Woman
Person:  type = [name: String, age: UInt, sex: Sex, address: String]
Result:  type t e = Ok t | Err e
```

标记落在槽位上,与箭头上的位标记(`!->`/`?->`,io-world.md §5.2)、绑定运算符(`=`/`:=`/`<-`,bind.md)是同一个设计动作。

## 2. 声明形

**代数型**——`|` 分隔变体,变体可带载荷。纵向书写每行以 `|` 引出,横向书写变体间以 `|` 分隔、无前导 `|`:

```eko
Sex: type =
    |Man
    |Woman

IoError: type = NotFound | Permission | Exists | NotFile
                 | Closed | InvalidData String | Other String
```

**记录型**——`[...]` 列出字段,字段名与类型以 `:` 相隔,字段间以 `,` 分隔;记录的值形为 `{...}`(四复合统一括号规则见 §3):

```eko
Person: type = [name: String, age: UInt, sex: Sex, address: String]

p: Person = {name: "ada", age: 36, sex: Woman, address: "x"}
```

**单变体**——一名一载荷的代数型,即新类型包装:

```eko
Age: type = Age UInt
```

## 3. 复合形(内建四复合)

列表、字典、元组、记录共用两对括号:**类型位 `[...]`,值位 `{...}`**,内部语法区分四形。记录声明与值形见 §2;本节补齐其余三形与统一规则。

### 3.1 类型位:`[...]` 内部语法

| 形 | 语法 | 例 |
|---|---|---|
| 列表 | `[]t`——空位 + 后随类型 | `[]Int`、`[]Person` |
| 字典 | `[k]v`——一键类型 + 后随值类型 | `[String]Int` |
| 元组 | `[a, b, …]`——≥2 个逗号类型 | `[String, Int]` |
| 记录 | `[f: a, g: b, …]`——名: 类型列 | `[name: String, age: UInt]` |

- 列表与字典由**内部**区分:空即列表,一型即字典;`]` 后拼接类型仅此两读。
- `[k]v` 读作 k 到 v 的映射;取值走 `d.get(k) -> Option v`(access.md)。
- 元组 **≥2 元**:一元 `(Int)` 是括号不是元组,零元 `()` 是零参函数参数组(io-world.md §5.3);单元是 `_`,不是零元组。
- 嵌套在必要处加括号:`[]([String]Int)` 是字典的列表,`[[]String]Int` 是列表作键的字典。

### 3.2 值位:`{...}` 内部语法

| 内部 | 形 | 例 |
|---|---|---|
| 逗号项(无冒号) | 列表 / 元组 | `{1, 2, 3}`、`{"first", 2}` |
| 冒号对 | 字典 / 记录 | `{"1": 1, "2": 2}`、`{name: "first", age: 2}` |

- **复合字面量永不独行**:`{1, 2}` 是列表还是元组,`{x: 1}` 是字典还是记录,由**期望类型**裁定——与 lambda 参数同机(lambda.md §1,类型由上下文给出);无期望类型处要求注解。**部分字面量**只在 `with` 实参位合法(derive.md §6):`p.with({age: 37})`——永不独行原则的唯一定点例外。
- 类型位四分,值位二分:列表↔元组同形(`{a, b}`),字典↔记录同形(`{k: v}`),期望类型收口。空字面量同规则:`xs: []Int = {}`。
- 值位元组不用 `(...)`:括号已被参数组、调用式、括号式占满;`{...}` 是值位的唯一复合括号。
- 集合已入 prelude 为 `Set t`:**无记号、无字面量**(`{a, b}` 已让给列表/元组),
  构造走 `Set.of`(stdlib.md §5.2);记号另立若痛。

### 3.3 逗号即元组

参数组、模式、字面、类型四处一体,箭头无 n 元与 tupled 之分:

- `(A, B) -> C` 的参数组即元组 `[A, B]`;
- lambda `(p, i):` 即二元参数组(`each_with_index` 回调即此形)——参数组同形于元组是书写统一,**不是自动解构**:一参回调吃进元组值时,拆开走绑定行 `(s, want) = row` 或 case 模式;
- 元组出现在非参数位(字段、元素、返回)时以 `[A, B]` 拼写:`mk: () -> [String, Int]`;
- `(p, i)` 模式(bind.md §3.2)解构元组值。

```eko
xs:     []Int          = {1, 2, 3, 4, 5}
counts: [String]Int    = {"1": 1, "2": 2}
pair:   [String, Int]  = {"first", 2}
p:      [name: String, age: Int] = {name: "first", age: 2}
Nums:   type = First | Second | Third

a: Nums = First
b: Nums = Second
```

## 4. 泛型

参数列于 `type` 之后;应用于类型表达式时并置;变量 snake_case,幻影、隐式泛化与全部裁定见 generics.md:

```eko
Result: type t e = Ok t | Err e

r:  Result Int String
xs: []Person                 // []t 是列表类型的前缀形
o:  Option Int
u:  Result _ IoError         // 类型位的 _ 即 Unit(io-world.md §5.3)
```

## 5. 编译期身份

类型活在编译期;运行时里生活的只有值。按类型分派的服务由编译期泛型承担:

```eko
show: (t) -> String          // prelude:按 t 实例化展开的纯函数
```

`Result`、`Option`、`IoError` 等由 prelude 提供(io-world.md §3),值随处可构造。

## 6. 命名规则(编译期强制)

类型与构造器 PascalCase(`Person`、`Ok`、`Man`),函数与类型变量 snake_case(`make_person`、`t`、`elem`,泛型见 generics.md),模块名 snake_case 即文件名(module.md §2)。违反即编译错误——大小写使名字分层结构化:PascalCase 是有身份的名字(声明型、构造器、命名空间),snake_case 是绑定(值域与类型域)与模块路径,消歧因此有结构基础(module.md §4.2)。构造器是不带括号的名字:值位 `Ok Man`,模式位 `Ok x -> ...`。

## 7. 名义与结构

主轴一句:**身份要显式买——声明即身份,匿名即视图。**与能力哲学
同构(权限显式声明,身份显式声明)。

### 7.1 声明即名义

每次 `type` 声明生成新类型;同形异名互不相通,转换显式(字面量
或逐字段重建)。无别名:RHS 只有声明形,命名不改身份。

```eko
Celsius:    type = [deg: Float]
Fahrenheit: type = [deg: Float]

f2: Fahrenheit = c              // ✗ 名义对名义,永不通
r:  Person = q_anon             // ✗ 匿名不冒充名义
r2: Person = {name: q.name, age: q.age}   // ✓ 重建入身份
```

温度事故结构性不可能;newtype 包装对一切声明型密不透风。

### 7.2 形适配:单向视图

名义值可流入**同形**匿名形——实参、操作数、期望类型三处生效;
这是无 traits 语言里数据形状解构的唯一机制(Go 接口满足同构):

```eko
foo: (p: [name: String, age: Int]) -> _ = …
bar: (h: Human) -> _ = …            // Human: type = [name: String, age: Int]

foo(p)      // ✓ p: Person 同形流入匿名视图
bar(p)      // ✗ Person ≠ Human
foo(pu)     // ✗ pu 的 age: UInt——非同形
q: [name: String, age: Int] = p     // ✓ 期望位适配
```

同形是精确匹配,不是宽度:少字段的匿名形不收多字段名义值,行多态
永禁。字段演进脆性即契约:Person 加字段,同形匿名签名全部断——
形状变了就该修。

### 7.3 两道封堵

- **无自动合取**:操作数定型以左为准,右须适配;混合名义操作数
  不寻公共匿名超型——`c == f` 以 Celsius 定型,Fahrenheit 不适配,
  温度事故从 eq 侧堵死。
- **位置纪律**:形适配不进容器元素与函数类型内部(与 `?`、Never
  的位置禁令同一精神);位适配(io-world §5.1)不受此限——位擦除
  故自由,形携身份故受限。逃生口:
  `ys: [][name: String, age: Int] = xs.map((p): p)`。

匿名 ADT 不存在(RHS 之外无联合形),形适配是记录与元组专属(元组形随 generics.md §4.1 延伸)。诊断:
`Human 不收 Person(皆为声明型);可经同形匿名形转场`。

## 8. 递归(2026-10 增订)

RHS 可引用被声明的名字——**递归型开门**;表示与内存定案于 repr.md:

```eko
Nat:    type = Zero | Succ Nat
Tree:   type t = Leaf t | Node [Tree t, Tree t, t]
Person: type = [name: String, friends: []Person]    // 递归可穿容器
Expr:   type = Num Int | Add [Expr, Expr] | Block []Stmt
Stmt:   type = Let [String, Expr] | Ret Expr         // 模块内互递归同权
```

- 自递归与互递归(模块内)同权——函数递归同款裁定(module.md 裁定
  7);跨模块类型递归随循环依赖禁令自动封死。
- **递归位恒间接**(repr.md 裁定 3):自引用处恒为指针,尺寸恒有限
  ——开门零新增检查。无基例的递归型(`Loop: type = Wrap Loop`)
  合法而零居留:居留性不作检查,与 `[]Never` 同精神(never.md §5)。
- derive(show/==/<)按结构递推,终止由堆图无环保证(repr.md §3
  环定理)。
- 形适配的同形判定在递归位按名相等:形状展开比对,环不进判定
  (§7 机器不动)。
