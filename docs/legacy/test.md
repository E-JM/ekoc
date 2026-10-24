# eko 测试 —— `test` 槽位与断言

状态:语法定案(2026-10)。结论先行,理由随文。
本文关闭 need.md 待定项"测试块:D5 承诺的测试语法/落点"。
2026-10 增订:派生表补 `In.from_lines`/`Env.frozen`/`Args.frozen`
(io-world.md §2 派生面补齐)。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | `test` 落注解槽:`name: test = 块`——与 `X: type = RHS` 同一设计动作 |
| 2 | 测试住在被测模块:同域直测私有面(`_` 前缀);跨文件测试 = 普通模块 + import,只及公开面;无测试模块种类 |
| 3 | 断言两词:`assert: (Bool) -> _`、`assert_eq: (t, t) -> _`;prelude 零类型自由函数,纯;失败即 panic |
| 4 | 失败消息走 `inspect`(结构通道);自定义失败 if + panic 组合,零新面 |
| 5 | `test` 绑定不导出、不进接口面;测试根 = 含 `test` 绑定的模块(module.md 裁定 16 落点) |
| 6 | 测试块无箭头:位无从对账,涉世调用照常求值;确定性靠派生能力,不靠位豁免 |
| 7 | 可测性派生零宿主(D5 兑现):`Out.null/tee`、`In.empty`、`Fs.null`、`Clock.frozen`、`Rand.with_seed`;真能力测试拿不到——构造权在宿主,测试无参数表 |
| 8 | 测试名 snake_case;模块内重名即编译错误;未用告警按全部编译根合计 |
| 9 | 禁:测试文件后缀、属性标注、describe/it、setup/teardown、mock 框架、参数化语法 |

## 1. 槽位

```eko
// person.eko
parse_rejects: test =
    case _parse_sex("x"):
        Err msg -> assert_eq(msg, "unknown sex: x")
        Ok sex  -> panic("unexpected: {sex}")

parse_smoke: test = assert_eq(_parse_sex("man"), Ok Man)
```

注解槽词汇表至此三词:

| 槽词 | 绑定的是 | 进接口面 | 编译条件 |
|---|---|---|---|
| (空) | 值 | ✓ | 恒 |
| `type` | 类型 | ✓ | 恒 |
| `test` | 测试 | ✗ | 测试根 |

`test` 与 `main` 同族——宿主契约绑定:`main` 仅入口模块(module.md
裁定 15),`test` 模模皆可;`main` 与 `test` 可同模块,两契约独立
编译。槽词皆 snake_case,与 PascalCase 类型分层(type.md §6),
故 `test` 非保留字:模块名 `test`、局部绑定 `test` 照常合法——
槽词只住在 `:` 后的注解位。

- 仅模块顶层;只走 `=`(测试声明一次,`:=`/`<-` 不适用)。
- 测试块是普通块(layout.md):任意表达式与绑定,值弃置;无箭头,
  位不对账(§0 裁定 6)。
- 测试名 snake_case(type.md §6 绑定规则),报告名 = `模块.测试名`;
  模块内重名即编译错误(与名字冲突一律错误同精神)。

## 2. 落点:被测模块

**无测试模块种类**——含 `test` 的模块就是普通模块,`test` 只是它
多出的一类顶层绑定。测试与被测代码同文件,三个直接后果:

- **私有面直测**:同一模块的 `test` 与顶层绑定同域,`_parse_sex`
  直接可调——私有函数本就模块内可见,同模块测试照用;不必为
  测试公开,接口面不因测试撒谎。
- **测试助手私有**:`_mk_person` 以 `_` 前缀活在被测模块,零导出
  污染;非测试编译下 `test` 绑定缺席,未用告警按全部编译根合计
  (测试根亦算),助手零告警。
- **跨文件测试 = 普通模块 + import**:想在他文件测,新建普通模块
  import 目标——只及公开面,目标的私有函数在彼处不可测(私有只
  在自家模块测);与生产代码同一纪律。

对照 `_test.eko` 后缀案(禁止):后缀约定造第二公民模块——私有
不可达,逼迫"为测公开";同文件即同真相。大文件之痛由拆模块解决
(module.md §2),不由拆测试解决。

## 3. 断言

```
assert:    (c: Bool) -> _ = if c: _ else: panic("assert failed")
assert_eq: (a: t, b: t) -> _ =
    if a == b: _ else: panic("assert_eq failed\n  left:  {inspect(a)}\n  right: {inspect(b)}")
```

- 两词入 prelude 开域(零类型自由函数,module.md 裁定 13 同款);
  纯(panic 不置位,B4);恒开启,零配置。
- `assert_eq` 同型锁死(derive.md §7);`==` 与 `inspect` 皆编译期
  泛型分派(derive.md §7)——capability 与函数无 `==`,断言即编译
  错误,如实。失败消息走 **inspect**:结构通道,永不可覆写,
  derive.md §1 的既有伏笔(使用处:诊断、测试、调试)就此兑现。
- 失败即 panic:测试运行的宿主按测试捕获 panic 记红——测试是
  panic 的第一个顾客,不是异常系统;正常编译 panic 照旧中止(B4
  不变)。
- 自定义消息:`if cond: panic(...)` 组合既有词汇;`assert_ne` 一类
  后补以失败消息质量为准另立,今日不配词。

## 4. 表驱动

参数化测试 = 列表 + `each`(分派者既有,位透明)。表字面量带注解
(复合字面量永不独行,type.md §3.2);`each` 回调一参——元组元素
不自动拆开,解构走绑定行:

```eko
parse_corners: test =
    rows: [][String, Result Sex String] =
        {{"man", Ok Man}, {"woman", Ok Woman}, {"x", Err "unknown sex: x"}}
    rows.each((row):
        (s, want) = row                    // 元组拆开在绑定行
        assert_eq(_parse_sex(s), want))
```

表即参数,each 即驱动;首个失败 panic 即止,该测试记红。`(p, i):`
一类多参回调(`each_with_index`)是参数组本形(type.md §3.3),
**不是解构**——拆元组永远显式:绑定行或 case 模式。

## 5. 能力与确定性(D5)

被测代码吃能力,测试喂派生能力——mock 即派生,不是框架:

| 派生 | 签名 | 行为 |
|---|---|---|
| `Out.null` | `() -> Out` | 吞掉一切写 |
| `Out.tee` | `(Out, (String) !-> _) -> Out` | 写一次、报一次——断言随流 |
| `In.empty` | `() -> In` | 读即 EOF(`read_line` → `Ok None`) |
| `In.from_lines` | `([]String) -> In` | 依次读行,尽即 EOF(2026-10 补) |
| `Fs.null` | `() -> Fs` | 一切路径 `Err NotFound` |
| `Env.frozen` | `([String]String) -> Env` | 恒读定表(2026-10 补) |
| `Args.frozen` | `([]String) -> Args` | 恒读定值(io-world §2.2) |
| `Clock.frozen` | `(Time) -> Clock` | 恒读定值,sleep 即刻(time.md §3) |
| `Rand.with_seed` | `(Int) -> Rand` | 种子定序;序列是实现细节,跨版本不承诺——断言域内性质,不断言具体值 |

`Out.tee` 签名随本档补定:构造纯;观察者随写触发,其位被写内建的
`!` OR 吸收(io-world.md §5.1 规则 5 的 `with_file` 同款),纯观察
者靠适配流入。

```eko
greet_lines: test =
    out = Out.tee(Out.null(), (s): assert_eq(s, "hi wd"))
    greet(out, "wd")
```

- **测试拿不到真能力**:构造权在宿主,`main` 的参数表是唯一入口
  (io-world.md §2),测试无参数表——派生组合子是测试的全部世界,
  能力层结构性确定。真 I/O 的集成测试属宿主域,另立。
- D5 第三承诺兑现:全部派生在 prelude 层完成,零宿主魔法。
- 位照常:喂的是真能力值,被测函数箭头位不变;测试不放宽任何
  检查——同编译器,同规则。

## 6. 根与运行

- **测试根 = 含 `test` 绑定的模块**。测试运行由宿主/工具发起
  (命令名属工具域):按根收集、逐根编译执行、逐测试捕获 panic;
  依赖闭包照常 DAG(module.md 裁定 16 的落点)。
- `test` 绑定不进接口面:分开编译无感;接口抽取跳过。
- 无测试的模块在测试编译下照常被依赖,不产生测试面——无 ambient
  测试,与无 ambient I/O 同一形状(A2)。
- 执行序与并行属宿主域;测试间零共享状态(值不可变,顶层纯),
  并行天然安全——顶层纯度(io-world.md §7 推论)的直接分红。

## 7. 禁止清单

- **测试文件后缀/测试模块种类**——`_test.eko` 一类后缀约定造第二
  公民模块,为测公开即接口撒谎;跨文件测试用普通模块 + import
  (§2),不需要种类。
- **属性标注(`#[test]`/装饰器)**——注解槽已开(`type` 先例),
  属性层为单一概念叠床。
- **describe/it/嵌套套件**——嵌套是叙事糖;表驱动 + `_helper`
  纯函数等价,零新面。
- **setup/teardown 钩子**——资源走 `with_file` 一类既有机器,构造
  走纯函数;钩子是隐式控制流。
- **mock 框架**——mock 即派生能力(§5);框架是能力系统的第二套
  词汇。
- **参数化语法**——表即参数,each 即驱动。
- **测试位豁免/宽检**——同编译器同规则;确定性靠值,不靠谎言。

## 8. 速查

```
parse_rejects: test = …         // 注解槽:test,住被测模块
assert_eq(got, want)            // 失败 panic,消息走 inspect
assert(n >= 0)                  // 布尔断言
case _parse_sex("x"): …         // 私有直测
rows: [][]t = {…}              // 表驱动:字面量带注解
(s, want) = row                // 元组拆开在绑定行
out = Out.tee(Out.null(), f)    // 断言随流
clock = Clock.frozen(t0)        // 测定;真 Clock 拿不到
```
