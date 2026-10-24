# eko 模块 —— 文件即模块,全名即路径

状态:语法定案(2026-10)。结论先行,理由随文。
2026-10 增订:泛型体随接口走(repr.md §6);§6 注记同步。
2026-10 增订:`Bytes` 入 prelude 开域清单(§7);定案见 bytes.md。
2026-10 增订:`Set`/`Json` 入 prelude 清单(§7);局部 `_x` 增未用安静义
(§5;stdlib.md、json.md、diag.md)。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 模块是编译期命名空间:装类型与自由函数;无状态、无初始化语义、无运行时足迹 |
| 2 | 文件即模块(源文件扩展名 `.eko`);目录是路径前缀,永不成为模块;全名 = 点分路径 |
| 3 | 依赖显式声明:import 清单即名称授权单;无隐式可见、无传递 re-export |
| 4 | import 把类型与构造器开进非限定域;函数永远全名访问 |
| 5 | UFCS 恒等式扩展:`x.Mod.f(args) ≡ Mod.f(x, args)` |
| 6 | 默认公开;`_` 前缀 = 模块私有;变体可见性随类型;导出签名中的类型必须可见 |
| 7 | 禁止循环依赖(编译错误);模块内递归绑定自由 |
| 8 | 接口自动从实现抽取,无接口文件;分开编译按接口,构建图即 DAG 缓存 |
| 9 | prelude 是唯一隐式模块,与用户模块同一冲突规则 |
| 10 | 名字冲突一律编译错误,无静默遮蔽——冲突是错误,不作警告降级 |
| 11 | 名字分层:表达式头绑定优先、模块兜底;内层遮蔽合法,import 与文件顶层同名即错误 |
| 12 | 大小写编译期强制:类型/构造器 PascalCase,绑定/模块 snake_case(type.md §6)——名字分层的结构基础 |
| 13 | prelude 隐式开域含零类型自由函数（`show`、`panic`、`assert`、`assert_eq`）；类型挂靠函数照走类型命名空间 |
| 14 | 文件名不得以 `_` 起头——模块可见性即 import 边,无私有模块层 |
| 15 | `main` 仅入口模块可绑定,非入口模块出现即编译错误 |
| 16 | 入口闭包之外的模块不参与编译，不是错误；测试根与 `test` 绑定定案于 test.md |

## 1. 定位:三问,不是五问

模块系统只回答三个问题——名字住在哪(单元)、谁能看见谁(可见性)、编译图怎么连(依赖)。传统模块系统的另一半难题已由既有正交机制分头解决:

| 难题 | 谁解决的 |
|---|---|
| 初始化顺序、import 副作用 | 顶层纯度免费(io-world.md §7 推论):模块体即纯常量 |
| 模块级状态、单例、连接池 | 能力值 + 捕获即委托(io-world.md §2,lambda.md §3) |
| 参数化模块(functor) | 泛型(io-world.md §6):类型参数化的唯一词汇 |
| 运行时链接 | 位与类型完全擦除(io-world.md §5.2):模块零运行时足迹 |

与 I/O 层同一设计动作:每个机制只解决一个问题。模块系统 = 命名与链接层。

"`main` 即授权单"的对称面:**import 清单即名称授权单**。eko 拒绝 ambient
authority,同样拒绝 ambient names——名字的来源与值的来源一样,显式声明。

## 2. 单元:文件即模块

一个源文件即一个模块,扩展名定案 `.eko`,文件名(不含扩展名)即
模块名;目录只是路径段,**永不成为模块**;模块全名 = 点分路径:

```
person.eko        → person
csv.eko           → csv
csv/lex.eko       → csv.lex
```

- 映射唯一:从全名反查文件零歧义。无"目录是模块还是容器"之问
  (Rust mod 树、Go 包名≠目录,两类历史歧义结构性消灭)。
- `csv` 与 `csv.lex` 无包含关系,只是路径相邻;各自独立 import。
- 大单元拆分:拆成子模块即可,facade **可选**——外界可直用
  `csv.lex.tokenize(l)`,也可经 `csv.eko` 的转发函数走短名。转发是
  函数,不是 re-export;诚实且可 grep。
- 重命名模块 = 移动文件 = 全局改全名引用(工具域)。

程序入口是含 `main` 绑定的模块;依赖图自入口传递闭包,宿主按 `main`
参数表授权(io-world.md §2.1),与模块图无关。三条边界随之而定:

- `main` 仅入口模块可绑定,非入口模块出现即编译错误;入口的
  `main` 必须公开(`_main` 同错)——`main` 是宿主契约(io-world.md
  D4),不是普通名字。
- 文件名不得以 `_` 起头。模块可见性**就是** import 边:没人 import
  即不可见,不存在更细的私有模块层;`_x.eko` 会暗示不存在的概念。
- 入口闭包之外的模块不参与编译，不是错误；测试编译以含
  `test` 绑定的模块为根（test.md）。

## 3. import:名称授权单

关键词定案 `import`(2026-10):Gleam 先例——import 与函数默认限定
调用是自然组合;`use` 落选(Rust 的 use 是开名入域,语义相逆),
`dep` 落选(包管理联想越层,包是工具域)。

import 列于文件顶部,先于其余顶层绑定;一行一模件:

```eko
import person
import stats
```

import 做两件事,且只做两件事:

1. **声明依赖**:该模块进入编译图。
2. **开域**:目标模块的**类型与构造器**进非限定域。

函数**永远**以全名访问——`person.parse(l)`、`stats.summary(ps)`。
理由即 call.md §4 的既有理由:全名让来源在调用点可见。

类型与构造器必须进非限定域:模式位是裸构造器(`Ok v ->`、`Man ->`),
类型注解是裸名(`p: Person`),这是既有定案的拼写,不可携带限定前缀。

```eko
import person

p: Person = person.parse({"ada", "36", "man", "x"}).unwrap()
case p.sex:
    Man   -> "M"
    Woman -> "F"
```

## 4. 调用与链式:全名的四个位置

链上每一环照常走 `.` 站点次序(call.md §3),其中**类型命名空间随类型
旅行、不随模块走**:`.split()`、`.map()`、`.try()` 在任何模块的链上
本地解析。跨模块全名在链上的四个位置:

| 位置 | 形 | 靠什么 |
|---|---|---|
| 链头 | `Mod.f(x)` | UFCS 原式(call.md §1) |
| 实参位 | `.map(Mod.f)` | 全名引用 |
| 元素级链中 | `.traverse((l): Mod.f(l.split(",")))` | 直接形,同链头 |
| 累积值链中 | `.Mod.f()` | **UFCS 恒等式扩展** |

### 4.1 UFCS 恒等式扩展

```
x.Mod.f(args)  ≡  Mod.f(x, args)
```

接收者位的函数名可以是全名。记号零新增(`.` 的既有组合),模块名
就在链上,溯源不丢;它补齐唯一原生不通的位置:

```eko
report <- persons
    .filter((p): p.age >= 18)
    .stats.summary()          // ≡ stats.summary(persons.filter(...))
    .show()                   // 类型命名空间站点,随 Summary 类型走
```

无此扩展时该位置只能拆变量或嵌套实参,管道线性断裂。

### 4.2 名字分层与消歧

import 是最外层词法域。三个句法位各一条规则:

| 位 | 规则 |
|---|---|
| 表达式头 | **绑定优先,模块兜底**:`people` 先查绑定(参数 → 局部 → 文件顶层),无绑定而 import 有该模块即模块路径。内层绑定遮蔽模块(内层遮外层,词法通则);**import 与文件顶层同名即错误**(同层,裁定 10) |
| 接收者位 | 站序:字段 → 类型命名空间 → 当前模块 → 模块路径(§4.1 即第 4 站);**永不查词法绑定**;后续解析失败列读法(call.md §3 机制) |
| 类型位 | 点分只能读作模块路径(类型位无值语义);限定形 `a.Person` 即两模块同名类型的消歧手段——裁定 4 的排压阀 |

大小写使分层结构化:类型/构造器 PascalCase、绑定/模块 snake_case,
编译期强制(裁定 12)。头位 PascalCase 必是命名空间或构造器;
snake_case 的绑定/模块之分由头位规则裁定。

两张逃逸牌:歧义处表达模块意图,直接形 `Mod.f(x)`(头位无绑定即
模块)与扩展形 `x.Mod.f()`(接收者位不查绑定)写出即是,皆遮蔽
免疫。局部遮蔽后,头位模块路径在该作用域不可达——改局部名解决,
不设别名机制。

```eko
import person
import stats

f: (rows: [][]String) -> []Person =
    person := rows.get(0)          // 局部遮蔽模块:合法
    person.parse(row)              // 错:[]String 无 parse;诊断列出模块读法
    rows.stats.summary()           // 对:接收者位不查绑定,走模块站
```

字段名撞模块名:`r.csv.split(",")` 字段站优先,后续解析失败列读法
择一;两模块同名函数同理,直接形零歧义。

### 4.3 类型命名空间的所有权

类型命名空间归**声明该类型的模块**所有,随类型可见性旅行:`Person`
的函数在 import person 后处处可经站点 2 解析(`p.make()`),无论
`make` 定义于哪个文件——只要 `Person` 声明在该模块。其他模块不得
向外来类型的命名空间添加函数;跨模块的服务以模块级函数 + 全名
(或 §4.1 扩展形)提供。预先堵死孤儿实例一类问题。

## 5. 可见性:`_` 前缀

默认公开;顶层绑定名以 `_` 起头即模块私有:

```eko
show_name:  (p: Person) -> String = …     // 公开
_show_name: (p: Person) -> String = …     // 私有:不进导出面
```

`_` 家族语义统一——"与公共命名域脱钩":类型位 `_` = Unit(无名类型),
值位 `_` = 丢弃,模式位 `_` = 通配,`_ := e` = 丢弃绑定,标识符首
`_` = 不进模块导出面。

配套三条:

1. **`_` 仅对模块顶层绑定生效**。局部绑定的 `_x` 拼写合法、语义中性
   (局部本无导出概念),且不参与 `W-unused` 未用告警(diag.md §3)——
   `_` 家族的"不在乎"义。
2. **导出签名中的类型必须可见**:公开函数的参数/返回引用私有类型
   `_Foo` 即编译错误——否则值能流动而类型名不可拼写,模式位写不出。
   私有类型只能活在私有签名里。
3. **变体可见性随类型,不可分**:公开代数型全变体公开(穷尽性依赖
   全集,同 IoError"变体一次配齐"之理);私有类型全私有。

得到的:零关键字、零清单、零双写,调用点即知公私。付出的,如实记
两笔:接口面无意识生长(eko 源码单仓编译,无二进制 API 冻结负担,
自动接口抽取可一览公开面);可见性变更即改名(IDE 一键,git 噪声
中性)。

## 6. 依赖图:DAG,无初始化语义

- **模块无初始化语义**。模块体是纯常量集合,按任意 DAG 顺序求值
  或直接常量折叠,顺序不可观察(io-world.md §7 推论的直接应用)。
- **禁止循环依赖**,二值错误(裁定 7 之于 B2)。模块内名互相引用
  (递归函数)自由;跨模块相互递归的解法是合并模块,或以函数值为
  参数传递——与能力流动同一形状。
- **接口自动从实现抽取**:类型、签名、可见性;无 .mli/.d.ts 人工层。
  分开编译按接口进行;无环 + 顶层纯,构建图即 DAG 缓存,增量编译
  天然成立。泛型函数的体随接口走(repr.md §6)——下游实例化在
  下游展开,同实例机器码链接期去重。

## 7. prelude

prelude 是唯一隐式模块(免 import,`List`/`String`/`Bytes`/`Set`/
`Json`/`Result`/`Option` 及全部能力类型)。

隐式开域补齐裁定 4 未言明的另一半：**零类型自由函数（`show`、
`panic`、`assert`、`assert_eq`）随 prelude 进非限定域**。`show` 首参是任意类型 `t`,挂不进
任何类型命名空间;不开则处处 `prelude.show(n)`,语言的拼写地板
塌了。类型挂靠的函数(`List.map` 一类)照走类型命名空间,开域对
它们无意义。开域集合封闭、与编译器共版,碰撞面有限;用户 import
永不全开(裁定 4),调用点来源可见不受影响。

冲突规则与用户模块同一:文件顶层绑定与开入名同层,同名即错误
(裁定 11);局部遮蔽照常合法。

## 8. 禁止清单

- **functor / 模块级签名**——泛型已占参数化词汇,两套词汇描述同一
  现象(与 io-world.md §9 标签集同罪)。
- **模块即值**——要值就用记录 + 闭包(`mk_logger` 已示范,
  lambda.md §3);模块不进值位。
- **open / 函数开进非限定域**——修复不了接收者位(三站点仍不含
  开入名,还得再添一条规则),且丢失调用点溯源、引入全局冲突面。
- **re-export**——转发是函数,转发不是改名;诚实且可 grep。
- **循环依赖**——DAG 是构建图、缓存与增量编译的地基。
- **条件编译**——多平台是宿主与正常代码的事,不是名字域的事。
- **初始化块 / 模块级副作用**——顶层纯度免费,不开后门(A2)。
- **跨模块扩展类型命名空间**——所有权归声明模块(§4.3);
  monkey-patching 与调用点溯源对立。

## 9. 完整示例

```eko
// person.eko ─────────────────────────────
Person: type = [name: String, age: UInt, sex: Sex, address: String]
Sex:     type = Man | Woman

parse: (data: []String) -> Result Person String =
    name    <- data.get(0).to_result("短行").try()
    age_s   <- data.get(1).to_result("短行").try()
    age     <- UInt.parse(age_s).map_err((e): "age: {e}").try()
    sex_s   <- data.get(2).to_result("短行").try()
    sex     <- _parse_sex(sex_s).try()
    address <- data.get(3).to_result("短行").try()
    Ok {name: name, age: age, sex: sex, address: address}

_parse_sex: (data: String) -> Result Sex String =
    case data:
        "man"   -> Ok Man
        "woman" -> Ok Woman
        _       -> Err "unknown sex: {data}"
```

```eko
// stats.eko ──────────────────────────────
import person

Summary: type = [count: UInt, mean: Float, oldest: Person]

summary: (ps: []Person) -> Summary =
    …
```

```eko
// report.eko ─────────────────────────────
import person
import stats

main: (out: Out, fs: Fs) ! =
    case fs.with_file("data.csv", Read, (file):
        content <- file.read(4096).map_err((e): "io: {e}").try()
        persons <- content.split("\n")
            .traverse((l): person.parse(l.split(",")))
            .try()
        report <- persons
            .filter((p): p.age >= 18)
            .stats.summary()
            .show()
        Ok out.println(report)):
        Ok _  -> _
        Err e -> out.println(e)
```

速查:

```
import person                    // 类型/构造器进非限定域
person.parse(row)                // 函数全名:链头/直接形
.map(person.parse_field)         // 函数全名:实参位
l.split(",").person.parse()      // ≡ person.parse(l.split(","))
p.stats._secret()                // 错:私有面不可达
csv.lex.tokenize(l)              // 路径即模块,无包含关系
```
