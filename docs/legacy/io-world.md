# eko I/O 层与世界位（World Bit）——设计定案

状态：语言设计定案（2026-10）。下一代编译器按本文实现；现行 `doc/` 与
`SPEC.md` 将依此重写。本文是裁定记录，格式沿泛型档（generics.md）先例：结论先行，
理由随文。语言按整体设计，不设迁移波次。
2026-10 修订：位变量 `E` 取消，开位匿名记号 `?`；`!`/`?` 与箭头连写
为 `!->`/`?->`（B1 修订，C4 收紧，新增表达式后缀 `!`/`?` 禁令；开位
两守则与反例清单随 B6 同批新定）。
2026-10 再修订：警告通道解禁。诊断三级：错误／警告／无；位域诊断
恒为错误（B2 不变）。
2026-10 增订：Range/Dict 服务入 §10，Enum 撤销——each/map 各归
类型命名空间。
2026-10 增订：Clock 三服务与时间三型定案于 time.md；`Clock.frozen`、
`Rand.with_seed` 入纯派生，`Range.fold` 入 §10；测试语法定案于
test.md；迭代与 TCO 承诺定案于 loop.md。
2026-10 增订:泛型档收编为 generics.md,类型变量拼写改小写;
§3/§5/§6/§8/§10/§12 签名示例同步。
2026-10 增订：二进制域定案于 bytes.md——`File.read_bytes/write_bytes`
入 §4，置位清单同步 §10。
2026-10 增订：`Args` 能力定案（D7），argv 入域；能力清单按出生配给／
主动开启重排，派生面补齐（§2、§10）——need.md 规范洞 argv 关闭。
2026-10 增订：Fs 补面入 §10 置位清单；JSON 派生入纯派生（stdlib.md、
json.md）——need.md 待定清零。

---

## 0. 裁定一览

| 编号 | 裁定 | 状态 |
|---|---|---|
| A1 | capability + world bit 为 I/O 层最终形态 | 定 |
| A2 | 干净断裂：删除全局 I/O，`main` 即授权单 | 定 |
| A3 | 语言按整体设计（新编译器，无波次分期） | 定 |
| A4 | 档案重写于 EkoDoc | 定 |
| A5 | "effect" 一词禁用；Unit 类型与值皆拼写为 `_` | 定 |
| B1 | 箭头三态 `->`/`!->`/`?->`；开位匿名，无位变量 | 定 |
| B2 | 过声明 = 编译错误（精确匹配；位域诊断恒为错误，不作警告） | 定 |
| B3 | EOF 嵌套：`Result (Option String) IoError` | 定 |
| B4 | panic / 发散 / 阻塞不入位 | 定 |
| B5 | CLI 默认授 `Out/In/Fs/Env`，`--sandbox` 收紧 | 定 |
| B6 | 开位两守则：由参数喂给、位置清单（§5.2；反例 §5.5） | 定 |
| C1 | 并发在终身目标内 | 定（方向） |
| C4 | 标签逃生门：永久关闭 | 定 |
| D1 | `OpenMode` 五变体 | 定 |
| D2 | 无 Rights 类型；`Fs.root` + `Fs.read_only` 纯组合子 | 定 |
| D3 | 句柄层：代际检查 + `with_file` | 定 |
| D4 | `main` 返回 `_`（缺省）或 `Int` | 定 |
| D5 | 嵌入三承诺 | 定 |
| D6 | 并发：`Conc.scope/spawn/join/cancel` 定案 | 定 |
| D7 | `Args` 能力：argv 入首发，出生配给默认面；能力清单重排＋派生面补齐（§2） | 定 |

## 1. 身份与总纲

eko 的目的是**诚实、简洁、一致**。I/O 层由四个正交机制组成，每个机制
只解决一个问题，不重复描述同一现象：

| 维度 | 机制 | 回答的问题 |
|---|---|---|
| Authority | capability 值 | 谁有权 |
| Honesty | world bit（`!`/`?`） | 这次调用会不会碰到世界 |
| Failure | `Result` 值 | 失败了怎么办 |
| Resource | 代际句柄 + `with_*` | 资源错用会怎样（受检，不 UB） |

核心洞见（本设计与效果行方案的分水岭）：**capability 已携带授权粒度；
world bit 是能力使用在箭头上的投影。** 两者共用一套词汇（能力名），
位不引入第二套词汇。标签集、行代数、handler 全部否决（§9）。

## 2. Capability 层

不透明、不可伪造、可复制的内建类型：

```
Out  In  Fs  Env  Args                // 首发·出生配给（§2 分层表）
Clock  Rand                           // 首发·主动开启
Net  Proc  Conc  Unsafe               // 目标集合，随网络/并发/FFI 波次
```

- 构造仅宿主/入口；无 ambient I/O：全局 `print`/`println`/`input`/
  `File.*` 删除，一切 I/O 从 `main` 的参数流下来。
- `main` 的参数即权限申请（只能是 capability 类型或空）；宿主按参数
  逐项授予。`main: ()`（纯程序）始终合法。
- `main` 的返回类型是 `_`（缺省）或 `Int`（D4）：`Int` 在 CLI 下即
  退出码（0/1/2 已被 ok/panic/编译错误占用），在嵌入下即宿主拿到的
  返回值。与 `Proc.exit`（abrupt，`-> Never`）不冲突。
- 能力可复制、可传递、可被闭包捕获；复制即委托。无 `==`，显示 `<cap>`。
- 衰减与组合子是**纯的**（元数据操作，不置位）：
  `Out.tee`、`Out.null`、`In.empty`、`In.from_lines`、`Fs.null`、
  `Fs.root`、`Fs.read_only`、`Env.frozen`、`Clock.frozen`、
  `Rand.with_seed`、`Args.frozen`（测定派生，见 time.md、test.md；
  派生面补齐见 §2）。需要检查世界状态用独立的置位内建（如
  `Fs.exists`）。
- CLI 默认授予出生配给五件 `Out/In/Fs/Env/Args`（脚本体验不倒退，
  安全默认关闭；`--sandbox` 收紧）；

**能力清单与授权分层**（2026-10 增订重排，D7）——主轴一句：
**出生配给是宿主启动你时给你的面；主动开启是你向世界要的源。**

| 能力 | 面 | 波次 | CLI 默认 | 世界接触 |
|---|---|---|---|---|
| Out | 会话写 | 首发 | ✓ | 置位：`print/println` |
| In | 会话读 | 首发 | ✓ | 置位：`read_line` |
| Fs | 文件系统 | 首发 | ✓ | 置位：`open/read/…` |
| Env | 环境变量 | 首发 | ✓ | 置位：`get` |
| Args | 启动参数 | 首发 | ✓ | 置位：`all`（§2.2） |
| Clock | 时间 | 首发 | ✗ 显式 | 置位：`now/mono/sleep` |
| Rand | 熵 | 首发 | ✗ 显式 | 置位：`int` |
| Net | 网络 | 目标 | ✗ | 置位（彼档） |
| Proc | 进程 | 目标 | ✗ | 置位（彼档） |
| Conc | 并行资源 | 目标 | ✗ | **零置位**（§8） |
| Unsafe | FFI／洞 | 目标 | ✗ | extern 默认置位（§5.1 规则 7） |

- **出生配给**（Out/In/Fs/Env/Args）：stdio、文件系统、环境、参数
  ——进程启动即携带的上下文。B5 默认授予的恰是这一层：默认面不是
  “常用的”，是“与生俱来的”；`--sandbox` 收紧。
- **主动开启**（其余）：观测或扰动启动配置之外的世界——时钟与熵
  （非确定源）、网络与进程（进程外）、并行（资源）、Unsafe（洞）。
  显式申请，默认关闭。
- **能力是世界接触的必要条件，不是充分条件**：Conc 全零置位
  （§8：scope/spawn 是 `?` 分派者，join/cancel 纯）——资源能力
  授权“可以做”，不担保“触世界”；置位由内建判据逐词裁定
  （§5.1 规则 2），不随能力名一刀切。
- **派生面补齐**（本批发现）：`In.from_lines`、`Env.frozen` 补齐
  测定对称性——逐能力配一枚定值构造器（Fs 的固定树另立若痛），
  测试确定性不再有死角（test.md §5）。

### 2.1 嵌入契约（D5）

语言层只承诺三件事；宿主侧 native API 是编译器细节：

1. **`main` 的参数类型表即授权协议**：宿主按位供给、逐项类型检查，
   缺项或多余即启动失败。
2. **能力不透明**：语言内永远只见 `<cap>`，构造权只在宿主。
3. **测试零宿主依赖**：`Out.null` 等派生在 prelude 层完成，不需要
   宿主魔法。

### 2.2 Args：argv 的正门（2026-10 增订，D7）

argv 是宿主世界的数据，读它即观测世界——正门是能力，不是魔法参数：

```
Args.all:    (Args) !-> []String       // 唯一门：全量参数，一次读出
Args.frozen: ([]String) -> Args        // 纯派生：恒读定值（测定）
```

```eko
main: (out: Out, args: Args) ! =
    case args.all():
        {cmd, ..rest} -> dispatch(out, cmd, rest)
        {}             -> usage(out)
```

- **为什么不走 `main: (argv: []String)`**：参数表即授权协议
 （D5：逐项类型检查，缺余即启动失败）——数据参数造第二种参数，
  协议两分；且 `main: ()` 的纯程序连“见过 argv”都不该成立，无
  ambient 原则一以贯之。Args 走既有机器：声明即申请，宿主按位
  供给，嵌入宿主自由合成自己的 Args。
- `all` 产**参数表**（不含程序名 argv[0]）：工具要的是参数；程序名
  属启动器叙事，嵌入宿主无此概念。
- **无衰减组合子**：Fs 的衰减服务持续通道的续授（子树、只读）；
  Args 读出即数据，通道随之无用。子命令转发即列表模式
  （上例）；后续处理是 `[]String` 上的纯计算。
- 标志解析（`--help` 一类）是 `[]String` 上的用户域库，不入语言。

## 3. 失败层

- `Result: type t e = Ok t | Err e`——内建封印但**可构造**（与 `Option`
  的唯一差别：错误处理的通货必须能造）。
- 可恢复失败一律 `Result`；`panic` 仅用于不变量违反。
- **EOF 不是错误**（B3）：`In.read_line: (In) !-> Result (Option String) IoError`，
  三臂穷尽 `Ok Some` / `Ok None` / `Err`。
- `IoError` 定案（2026-10）：穷尽匹配的封闭 union，**变体必须一次配齐**
  ——后补变体破坏所有既有 match。判据：调用方会不同分支处理才配得名。

  ```
  IoError: type =
      | NotFound            // 路径不存在
      | Permission          // 权限拒绝
      | Exists              // 独占创建撞已存在（OpenMode.New）
      | NotFile             // 目录或其他非普通文件
      | Closed              // 句柄已关闭（代际检查，§4）
      | InvalidData String  // 内容无效（如非 UTF-8），载荷为细节
      | Other String        // 其余，载荷为系统消息
  ```

  附则：运行时内部重试 EINTR，`Interrupted` 永不浮出；作用域为
  console/fs/句柄，未来 Net 另立错误 union，不回流污染。
- `panic(msg) -> Never` **纯**（B4）；`Option.expect`/`unwrap` 纯；
  除零、溢出 panic 不置位(索引越界已非 panic——索引即域检查,access.md)。

## 4. 资源句柄层（D1/D3）

```
OpenMode: type = Read | Write | Append | New | Update
//  Read    打开已有文件读，缺失 → Err NotFound
//  Write   创建或截断
//  Append  创建或追加
//  New     仅当不存在时创建（独占），已存在 → Err Exists
//  Update  读且写，不截断（就地改写）

Fs.open:         (Fs, String, OpenMode) !-> Result File IoError
File.read:       (File, Int) !-> Result String IoError    // 顺序读至多 n 字符;非 UTF-8 → Err InvalidData
File.read_bytes: (File, Int) !-> Result Bytes IoError     // 顺序读至多 n 字节;永不 InvalidData(bytes.md)
File.write:      (File, String) !-> Result _ IoError      // 写在当前位置
File.write_bytes:(File, Bytes) !-> Result _ IoError       // 字节写在当前位置(bytes.md)
File.flush:      (File) !-> Result _ IoError
File.close:      (File) !-> Result _ IoError              // 陈旧句柄 → Err Closed
with_file:       (Fs, String, OpenMode, (File) !-> a) !-> Result a IoError
```

政策五条：

1. **代际检查**：`close` 将槽位代际 +1，所有旧副本失效；对陈旧句柄
   的任何操作（含再次 close）得 `Err Closed`——double-close /
   use-after-close 是受检错误，不是 UB。
2. **泄漏允许**：不 close 则进程退出由 OS 回收；finalizer 不作承诺。
3. `with_file` 在值/错误两条路径都关闭；panic 是 abort，OS 回收，
   无需 unwind。
4. 回调类型是 `(File) !-> a`，**无开位**：`with_file` 自身涉世，OR
   吸收（§5.1 规则 5）。
5. seek / 流式按行推迟：`File.read(f, n)` + `String.lines` 足够。

逃逸示例（代际保证的展示）：

```
sneaky: (fs: Fs) !-> Result File IoError =
    with_file(fs, "x", Read, (f): f)        // 返回的句柄已被 with_file 关闭
// 之后对它的任何 File.read → Err Closed，受检，不 UB
```

衰减（D2）：**无 Rights 类型，组合子即权利**——没人会去匹配的权利
数据是死的；两个纯组合子可组合，覆盖全部衰减场景：

```
Fs.root:      (Fs, String) -> Fs    // 纯：限定子树，不查存在性
Fs.read_only: (Fs) -> Fs            // 纯：收回写权
// read_only(Fs.root(fs, "/tmp"))；Net 将来同构：Net.hosts / Net.ports
```

## 5. World Bit 层

### 5.1 语义七条

1. **位置 = 体内全部调用位的 OR；只有调用贡献。** 持有不置位：引用
   涉世函数、存储能力值、传递函数值都不置位。
2. **内建判据一句话：观测或改变世界的内建调用置位；纯派生不置位。**
   `Out.println`、`Clock.now` 置位；`Out.tee`、`Fs.root` 纯。
3. **精确匹配**（B2）：注解绑定处声明的位必须与体的位相等——缺 `!`
   与多 `!` 都是编译错误（死注解与死代码同罪）。子类型
   `(a) -> b <: (a) !-> b` 只发生在**适配位置**：实参、操作数、
   期望类型、集合元素。
4. **位住在箭头上，不感染数据。** `logged := List.map(xs, greeter)`
   的调用要求外围 `!`，但 `logged : []String` 是普通值——
   没有"涉世的列表"。
5. **`?` 只出现在"回调纯则自身纯"的分派者**（map/each/fold 这类
   迭代器）。自身涉世的 HOF 无需 `?`：`with_file` 已是 `!->`，
   `OR` 吸收一切，回调直接写 `(File) !-> a`。
6. **一个签名内全部 `?` 是同一个开位。** bit 上 OR 吸收，多变量无
   区别——`?` 因此匿名：签名里不存在位变量，名字没有消歧工作。
7. **extern 默认置位。** 纯 extern 是审计过的人工契约，仅 prelude
   可声明（保住"涉世 ⟹ 用了能力"的定理）。

### 5.2 语法（B1）

```
(a) -> b                    // 纯：保证不接触世界（可 panic/发散，§7）
(a) !-> b                   // 定涉世：可能接触世界
(a) !                       // Unit 返回的省略箭头形式（≡ (a) !-> _）
(a) ?-> b                   // 开位：回调纯则实例化为纯，否则涉世
(a) ?                       // 开位 Unit 返回的省略箭头形式（≡ (a) ?-> _）
([]t, (t) ?-> u) ?-> []u    // 分派者：签名内全部 ? 是同一开位
```

- 读法一行：`?` 是位上的匿名参数，与类型变量 `t` 同机（§6）——开位由参数喂给、
  在调用点按实参实例化。三箭头合为“两个位 + 一个参数”的图景：`->` 与
  `!->` 是两个位，`?->` 是位的透传形。
- `!`/`?` 与箭头**连写为单记号** `!->`/`?->`，贴各自箭头，无绑定
  规则。嵌套两方向均无需括号：

```
(Out) -> (String) !-> String      // 内层涉世，外层纯
(Out) !-> (String) -> String      // 外层涉世，内层纯
(Out) !-> (String) !-> String     // 两层涉世
```

- 记号自锁，无需排版规则：`!->`/`?->` 由最长匹配成词；`!=` 仍是
  表达式比较符，类型位无歧义。单写 `!`/`?` 只出现在省略箭头形
  （`(a) !` ≡ `(a) !-> _`，`(a) ?` ≡ `(a) ?-> _`）。
- 文法：`ParenGroup ('!' | '?')? ('->' Type)?`；标记只允许出现在
  可接箭头的参数组之后。布局算法零改动（连写记号永不折行；单写
  标记仅见于省略箭头形，即行尾）。
- **位完全擦除**，零运行时开销；不参与类型相等之外的任何调用约定。
- **开位匿名，无位变量。**一个签名内全部 `?` 是同一个开位（规则 6），
  名字没有消歧工作；`?` 于注解处泛化、调用点按实参位实例化、lambda
  实参最后检查时收集。**开位必须由参数喂给**：自身箭头为 `?->` 仅当
  至少一个参数类型含 `?` 箭头（同 generics.md 的 result-only 禁令，
  别名展开后判定）。**新编译器自始对用户开放**（原 builtin-first
  分期随 A3 作废）。
- **`?` 只出现在两类位置**：被注解绑定自身的箭头，与该签名参数类型
  中的箭头；返回类型内部、容器元素、字段等位置禁止——那里没有绑定
  在泛化，开位无从实例化，只剩假精确。其余位置只有 `->` 与 `!->`。

### 5.3 Unit 与 `_`（A5）

- **Unit 的类型与值拼写皆为 `_`；`Unit` 名称退役。** 拼写归一：
  - 类型位置：`_` = Unit（`Result _ IoError`、`Option _`）；
    `()` 只剩一个含义——零参函数类型（`() -> _` 的省略形）。
  - 值位置：`_` = unit 值：`Err Eof -> _`、`Ok _` 构造 `Result _ IoError`；
    `()` 从值位置退役，只在 `f()` 调用语法里出现。
  - 模式位置：`_` = 通配。Unit 恰只有一员，通配与 unit 字面模式
    外延相同——`Ok _` 无论按哪种读法行为一致，碰撞不存在。
  - 显示亦为 `_`：所见可所写，不为显示开第二种拼法。
- `_` 在类型位置**只**意味着 Unit，永远不是"请推断"——eko 的注解
  没有推断洞。文档第一段须明示，防 Rust/TS/Scala 迁移误读。
- `_` 不得作 lambda 参数名（参数是标识符）；`_ := e` 丢弃绑定形式
  不变，与值 `_` 靠 `:=` 语法区分。
- 动机（现行 spec 的真实洞）：按原 §4 文法 `-> ()` 解析为"返回零参
  函数"，Unit 返回原本**无法显式写出**；`-> _` 补上显式形，省略形
  的糖从此可显式往返：`foo: (a: Int) -> _ = ...` ≡ `foo: (a: Int) = ...`。
- `(a) !-> _` 是涉世 Unit 返回的显式形。

### 5.4 术语（A5）

"effect" 一词从全部文档与诊断中禁用。`(t)` = **Unit 返回函数类型**;
`!` = **world 标记**；`?` = **开位标记**；位 = **world bit**；
`!->`/`?->` = **涉世/开位箭头**。

### 5.5 反例清单（B6 验收面）

合法形态唯一：**参数喂位，体透传位**。教学口径：写 `->` 与 `!->`
不需要理由，写 `?` 需要——你得真的把回调的位透传出来。prelude 里
`?` 只属于分派者（§10），用户代码里它应当是罕见词。

```
apply_twice: (f: (Int) ?-> Int, n: Int) ?-> Int = f(f(n))
// 对：体位 = 开（两次调用 f，OR 幂等）= 声明位

inc: (n: Int) -> Int = n + 1
a := apply_twice(inc, 5)    // 纯实参 ⟹ 纯实例化，调用不要求外围 !
b := apply_twice((n): Out.println(out, show(n)); n + 1, 5)
                            // 涉世实参 ⟹ ! 实例化，调用要求外围 !
```

对账四向皆错，不存在"标个 `?` 保险一下"：

```
count: (xs: []Int) ?-> Int = List.length(xs)
// 错：无参数含 ?，开位无人喂；体位 = 纯 ≠ 开。改：(xs: []Int) -> Int

loud_apply: (out: Out, f: (Int) ?-> Int, n: Int) ?-> Int =
    Out.println(out, "call"); f(n)
// 错：体位 = 开 ∨ ! = !（! OR 吸收）。
// 改：(out: Out, f: (Int) !-> Int, n: Int) !->（§5.1 规则 5，纯回调靠适配）

print_all: (out: Out, xs: []String) -> _ =
    List.each(xs, (s): Out.println(out, s))
// 错：each 以 ! 实例化，体位 = ! ≠ 纯；且碰世界必须持有能力值（§2）

twice_fast: (f: (Int) ?-> Int, n: Int) -> Int = f(f(n))
// 错：体位 = 开 ≠ 纯
```

result-only 禁令与别名洗白（同一条，展开后判定）：

```
double: (xs: []Int) ?-> []Int = List.map(xs, (n): n * 2)
// 错：无参数含 ?；map 以纯实例化，体位 = 纯。改：(xs: []Int) -> []Int

Cb: type = (Int) ?-> _
c: Cb = (n): n + 1                      // 错：展开即自身箭头 ?，无参数喂
run: (f: Cb, n: Int) ?-> Int = f(n)     // 对：Cb 在参数位，喂得起
```

位置清单（§5.2 第二守则）：

```
handlers: [](Int) ?-> Bool = {isZero, isNegative}
// 错：? 在容器元素位，无绑定在此泛化。全纯 → [](Int) -> Bool；
// 掺涉世 → [](Int) !-> Bool（纯元素靠适配放入）

make: (seed: Int) !-> (Int) ?-> Int
// 错：? 在返回类型内部；返回函数的位创建即定，要开的是调用方的绑定
```

对照——持有不置位，`?` 的纪律不传染：

```
mk_logger: (out: Out) -> (Int) !-> Int =
    (n): Out.println(out, show(n)); n + 1
// 对：只构造不调用，工厂纯（§5.1 规则 1）；返回 ! 闭包的纯函数处处合法
```

## 6. 与泛型的整合

generics.md（收编档）全部裁定不变——变量拼写小写随收编改定，本文
签名示例同步。增量三条：`?` 的位置 kind 规则（§5.2）；
`?` 与类型变量同机泛化/实例化（匿名进行）；幻影检查看穿 `?` 位置
（`Callback: type t = (t) ?-> _` 合法，开位经 RHS 的 `?->` 出现）。

## 7. 定理与非承诺

> **World soundness**：若 safe eko 中 `f : (a) -> b`（无 `!`），
> 则调用 `f` 不产生任何世界效果。
> 骨架：置位仅源于要求能力值的内建调用；能力不可伪造，只能经
> `main`/宿主构造或纯派生；闭包的位在其箭头类型上；子类型单向。

**推论**：顶层初始化纯度免费成立——顶层没有能力值，置位调用结构性
不可能。

**非承诺**（文档必须并列写）：无 `!` ≠ 不 panic、≠ 终止、≠ 不阻塞、
≠ 不被取消。位只管世界接触，部分性是独立维度。

## 8. 并发（D6，定）

以下四条签名与推导为定案。channel / 超时不在本裁定内——若将来引入，
是另一次裁定，且必须重审确定性论证（引入通信即引入调度可观察性）。

```
Conc.scope:  (Conc, (Scope) ?-> a) ?-> a          // 纯回调 ⟹ scope 调用纯
Conc.spawn:  (Scope, () ?-> b) ?-> Task b         // 子任务世界位并入开位
Conc.join:   (Task b) -> Option b                 // 纯：None = 已取消
Conc.cancel: (Task b) -> _                        // 纯：程序内部状态
```

推导（为何 `scope` 开位而非定死 `!->`）：值不可变 + v1 无任务间
通信 ⟹ 任务结果在 spawn 那一刻已定，调度序不影响任何结果；取消只有
显式来源（父 `cancel` 或 scope 退出），程序序确定 ⟹ **纯任务无数据
竞争且确定**，`scope` 因此与 `List.map` 同为 `?` 分派者，不是特例。
`join`/`cancel` 观察程序内部状态，不触世，纯——"观测世界才置位"
的机械推论。附带：无 channel ⟹ 无死锁；取消不是错误，`join` 返回
`Option b`。

边界（诚实）：取消在世界调用边界生效，**不终止的纯任务会挂住 scope
退出**（与 B4"发散不跟踪"一致，不是漏洞）；channel / 超时是将来另
一次裁定，引入即破坏确定性论证。

## 9. 禁止清单

- **标签集**——与能力 1:1 镜像，两套词汇描述同一现象（`console`↔`Out`、
  `fs`↔`Fs`），永久同步负担。
- **箭头标记后加内容（标签、位变量）**——C4 永久关闭。预留不省未来
  成本（成本在语义与迁移，不在记号）；标签与能力的镜像是结构性事实，
  各层（并发/FFI/审计/测试/优化）均有更好答案。若未来需要新的效果
  机制，另立语法，不寄生于此标记。`!`/`?` 之后永远没有任何内容。
- **行多态 / 行代数 / 行扩展**——bit 使之无必要。
- **代数效应 / handler**——世界 I/O 不能被"处理掉"；native
  continuation 昂贵；属另一特性。
- **IO monad**——丢直书风格，HOF 全部 monadic 化。
- **异常入位 / panic 入位**——错误是值（`Result`），部分性是独立维度。
- **用户自定义标签**——滑向代数效应的斜坡。
- **位诊断降级为警告**——位域诊断恒为错误（B2）；一般诊断允许
  警告（2026-10 修订解禁，原“警告通道”全面禁止撤回）。
- **ambient I/O**——授权只能从 `main` 流下来。
- **"effect" 术语**——见 §5.4。

## 10. prelude 置位清单（验收面）

| 类别 | 签名 |
|---|---|
| 置位 `!` | `Out.print/println`、`In.read_line`、`Fs.read/write/append/read_bytes/append_bytes/exists/open/list_dir/mkdir/mkdir_all/rm/rm_dir/rename/stat`、`File.read/read_bytes/write/write_bytes/flush/close`、`with_file`、`Env.get`、`Args.all`、`Clock.now/mono/sleep`、`Rand.int` |
| 纯派生 | `Out.tee`、`Out.null`、`In.empty`、`In.from_lines`、`Fs.null`、`Fs.root`、`Fs.read_only`、`Env.frozen`、`Clock.frozen`、`Rand.with_seed`、`Args.frozen`、`show: (t) -> String`、`to_json/from_json`（json.md） |
| 纯得出人意料 | `panic(msg) -> Never`——中断不是世界效果 |
| `?` 分派者 | 一切接受回调的内建：`List.map/filter/fold/each/any/all/find_by/sort_by/zip_with`、`Dict.map/filter/fold/each`、`Range.each/each_with_index/map/fold`、`Option.map/and_then/or_else/filter`、`Result.try/map/map_err/traverse`（`<-` 反转绑定的组合子，bind.md §4） |
| 纯 | `List.length/get/reverse/append/...`、`String.*/append/chars/from_chars/at/length/from_char`、`Char.to_code/from_code`、`Bytes.*`(bytes.md)、`Int.*`、`Float.*`、转换族(`to_int/to_uint/to_float/parse`,convert.md)、`Option.unwrap/unwrap_or/expect/get/to_result`、`Dict.insert/remove/get/merge`、`Range.to_list/length` |

教学对照：`Option.expect` 纯而 `Option.map` 是 `?` 分派者——panic 不置位、
回调置位，两条规则一对照可教。

## 11. 未决项

无。I/O 层四个机制（authority / honesty / failure / resource）与并发
签名全部闭环。channel / 超时等扩展若将来引入，另立裁定，不在本档。

## 12. 语法速查

```
main: (out: Out) !                         // 涉世入口，授权即参数
main: ()                                   // 纯入口
main: (out: Out) !-> Int                   // D4：返回值即退出码/嵌入结果
f: (a: Int) -> _  ≡  f: (a: Int)           // Unit 返回，糖可往返
f: (a: Int) !-> _                          // 涉世 Unit 返回，显式
g: (Out) -> (String) !-> String            // 嵌套：位贴各自箭头
List.map:  ([]t, (t) ?-> u) ?-> []u        // 开位，纯分派者
List.each: ([]t, (t) ?-> _) ?              // 开位 Unit 返回，省略箭头形
h: [(Int) !-> Bool] = {isZero}             // 适配位子类型：纯入涉世
w: (f: File) !-> Result _ IoError          // _ 在类型位 = Unit
case File.write(f, "x"):
    Ok _ -> Out.println(out, "ok")         // Ok _：unit 值构造
with_file(fs, "log.txt", Append, (f): File.write(f, "entry\n"))
case In.read_line(in):
    Ok Some line -> echo(in, out, line)    // EOF 不是错误
    Ok None      -> Out.println(out, "bye")
    Err err      -> Out.println(out, "io: {err}")
```
