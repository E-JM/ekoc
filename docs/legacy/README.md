# EkoDoc —— eko 语言档案

诚实、简洁、一致。结论先行,理由随文;裁定记录,修订随文加注。

## 档案

| 档 | 主题 | 定案要点 |
|---|---|---|
| layout.md | 排版 | 缩进块、括号折行、case 臂 |
| bind.md | 绑定 | `=` `:=` `<-` 三绑定,反转绑定即回调 |
| lambda.md | 匿名函数 | 零标注,捕获即委托 |
| branch.md | 分支 | case 原语,if 是糖 |
| type.md | 类型 | `type` 槽位,代数型,四复合,名义与形适配 |
| generics.md | 泛型 | type 后 binder,并置应用,隐式泛化,变量小写 |
| repr.md | 值表示 | 装箱与 O(1) 共享,递归位恒间接,环定理,引用计数,单态化 |
| pattern.md | 模式 | 记录子集,or,列表 rest,多 scrutinee |
| never.md | Never | 底类型,必不返回,穷尽靠模式 |
| expr.md | 表达式 | 运算符糖,字面量,插值,注释 |
| derive.md | 派生服务 | show/inspect 双通道,== 与 < 派生,with,to_json/from_json |
| json.md | JSON | Json ADT,外标形,派生同构与覆写,往返保真 |
| char.md | 字符 | Char 标量,String 元素域,索引诚实 |
| convert.md | 转换 | 失败三分法,parse 族,往返性质 |
| call.md | 调用 | UFCS,站点次序,跨模块全名与接收者扩展 |
| access.md | 取值 | get 唯一入口,Option 钻取,信任在消费处 |
| io-world.md | I/O 层 | 能力与出生配给分层,world bit,Result,代际句柄,Args |
| bytes.md | 二进制 | Bytes 内建,字节 I/O,UTF-8 编解码,hex,与 FFI 解绑 |
| stdlib.md | 标准库 | Fs 补面,格式化函数族,Float 数学 IEEE,Set 无记号,键域,稳定排序 |
| time.md | 时间 | Time/Mono/Duration 名义三型,Clock 三服务,frozen 测定 |
| loop.md | 迭代 | 无 loop 语法;组合子与自尾递归,TCO 分层承诺 |
| module.md | 模块 | 文件即模块,import 授权单,`_` 前缀私有 |
| test.md | 测试 | `test` 槽位,断言即 panic,mock 即派生能力 |
| diag.md | 诊断 | 英文消息,稳定符号码,封闭警告清单;修订:运行时 P- 码族(09 §7.6) |
| ffi.md | FFI(定案档案) | 三向分析;裁定 F1–F23;已升级为规范章 11-ffi.md 并回写各章 |
| need.md | 待定 | 已清零;新洞先立条目 |

## 阅读次序

排版 → 绑定 → lambda → 分支 → 类型 → 泛型 → 值表示 → 模式 → Never → 表达式 → 派生 → JSON → 字符 → 转换 → 调用 → 取值 → I/O 层 → 二进制 → 标准库 → 时间 → 迭代 → 模块 → 测试 → 诊断。
FFI 裁定档案(ffi.md)已升级为 11-ffi.md;阅读规范时无需回看,查阅裁定理由时用。
