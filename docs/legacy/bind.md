# eko 绑定 —— `=` `:=` `<-`

状态:语法定案(2026-10)。结论先行,理由随文。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 统一绑定文法,行首是名字或模式 |
| 2 | `=` 初次绑定,`:=` 重新绑定,`<-` 反转绑定 |
| 3 | `p <- f(a)` ≡ `f(a..., (p): rest)`,rest 是剩余块 |
| 4 | `<-` 右侧是调用形,回调位于末位 |
| 5 | `<-` 记号由反转绑定专用 |
| 6 | 数据流向:case 臂 `->`,绑定 `<-` |

## 1. 行首不变量

每一行从名字、模式或表达式开始；标记落在槽位上——注解槽（`type`/`test`，type.md、test.md）、箭头（`!->`/`?->`，io-world.md §5.2）、绑定运算符（本文）。

## 2. 统一文法

```
PATTERN (':' 注解)? '='    右侧        // 初次绑定
PATTERN ':='              右侧        // 重新绑定
PATTERN '<-'              调用         // 反转绑定
```

注解随 `=` 出现;`:=` 沿用原绑定之名,`<-` 的类型由回调参数给出。

## 3. 反转绑定 `<-`

### 3.1 脱糖

```
p <- f(a...)
rest
```

即:

```
f(a..., (p): rest)
```

rest 是本行之后的整个剩余块。回调位于末位是 prelude 的既有契约(`with_file`、`List.each`、`Result.try` 一致),`<-` 与它对齐(call.md §2)。该行的值即调用的值(如 `Task b`);同时取得调用值与反转时,直接书写嵌套 lambda 形即可。

### 3.2 形态

**模式**取变量、元组、记录三种绑定形:

```eko
age <- data.get(1).to_result("短行").try()
(p, i) <- persons.each_with_index()
```

**零参回调**省去模式:

```eko
<- scope.spawn()            // rest 即任务体(io-world.md §8)
```

**多个 `<-`** 依次嵌套,后行位于前行的回调体内;**case 臂内**,rest 是该臂的剩余块。

### 3.3 位

`<-` 是排版糖:rest 即回调体,位按 io-world.md 推断(lambda.md §2)。分派者按 rest 实例化开位(io-world.md §5.1 规则 5);`with_file` 一类回调签名定型为 `!` 的,以 OR 吸收(io-world.md §4)。

### 3.4 数据流向记法

case 臂里箭头指向结果(`模式 -> 值`);绑定行箭头指向名字(`调用 <- 模式`)。箭头方向即数据方向。

## 4. 与 Result 组合子

prelude(io-world.md §10 增补):

```
Result.try:      (Result t e, (t) ?-> Result u e) ?-> Result u e
Result.map:      (Result t e, (t) ?-> u)          ?-> Result u e
Result.map_err:  (Result t e, (e) ?-> f)          ?-> Result t f
Result.traverse: ([]t, (t) ?-> Result u e)        ?-> Result []u e
```

接收者形(call.md):`expr.try()`、`lines.traverse(f)`、`expr.map_err(g)`。

`<-` 向调用追加末位回调,因此链上每一段 Result 的解开都以 `.try()` 显式
出现;链上各环节的错误类型一致,`map_err` 是转换点。

`<-` 链的收尾表达式落在最后一个组合子的回调返回形上;`try` 链以 `Ok ...` 收尾:

```eko
age_s <- data.get(1).to_result("短行").try()
age   <- UInt.parse(age_s).map_err((e): "age: {e}").try()
Ok age
```

## 5. 完整示例

```eko
parse_sex: (data: String) -> Result Sex String =
    case data:
        "man"   -> Ok Man
        "woman" -> Ok Woman
        _       -> Err "unknown sex: {data}"

make_person: (data: []String) -> Result Person String =
    name    <- data.get(0).to_result("短行:缺 name").try()
    age_s   <- data.get(1).to_result("短行:缺 age").try()
    age     <- UInt.parse(age_s).map_err((e): "age: {e}").try()
    sex_s   <- data.get(2).to_result("短行:缺 sex").try()
    sex     <- parse_sex(sex_s).try()
    address <- data.get(3).to_result("短行:缺 address").try()
    Ok {name: name, age: age, sex: sex, address: address}

main: (out: Out, fs: Fs) ! =
    case fs.with_file("data.csv", Read, (file):
        content <- file.read(4096).map_err((e): "io: {e}").try()
        persons <- content.split("\n").traverse((l): l.split(",").make_person()).try()
        Ok persons.each_with_index((p, i): out.println("{i}: {p}"))):
        Ok _  -> _
        Err e -> out.println(e)
```
