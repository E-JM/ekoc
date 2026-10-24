# eko 排版 —— 块与折行

状态:语法定案(2026-10)。结论先行,理由随文。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 块由缩进定义:`:` 开块,成员同列,嵌套进一级 |
| 2 | 括号内折行自由,配对 `)` 收口;收口处同时结束其中的回调块 |
| 3 | 末位回调可独占一行,回调体再进一级 |
| 4 | 链式续行以 `.` 起头,同列对齐 |
| 5 | case 臂是 case 的块(深于 `case` 一级);scrutinee 折行深于 `case` |

## 1. 块:缩进与冒号

函数体、case 臂、if 分支、回调体都是块。`:` 开块;块成员同列起行,嵌套块再进一级:

```eko
main: (out: Out, fs: Fs) ! =
    content <- …
    …
```

## 2. 括号内折行自由

括号内的换行是排版自由,配对的 `)` 收口,收口前的行属于同一调用。两种常用姿势:

```eko
fs.with_file(
    "data.csv",
    Read,
    callback)
```

```eko
fs.with_file("data.csv", Read,
    callback)
```

末位回调独占一行是它的常用形:回调头 `(p):` 一行,体再进一级:

```eko
persons.each_with_index(
    (p, i): out.println("{i}: {p}"))
```

括号收口处同时结束其中的回调块。

## 3. 链式折行

接收者链(call.md §5)以 `.` 起头折行,续行同列:

```eko
content.split("\n")
       .map(String.trim)
       .filter((l): l != "")
       .length()
```

## 4. case:scrutinee 与臂

scrutinee 折行深于 `case`;臂是 case 的块,深于 `case` 一级。scrutinee 含跨行回调时,收口行以 `):` 结束,臂从下一行开始:

```eko
main: (out: Out, fs: Fs) ! =
    case fs.with_file("data.csv", Read,
            (file):
                content <- file.read(4096).map_err((e): "io: {e}").try()
                persons <- content.split("\n").traverse((l): l.split(",").make_person()).try()
                Ok persons.each_with_index((p, i): out.println("{i}: {p}"))):
        Ok _  -> _
        Err e -> out.println(e)
```

结构视图:

```
case [ fs.with_file(路径, 模式, (file): 回调体) ] :
    臂…
```

## 5. 嵌套的来源(注)

case 圈住含回调的调用是语义使然:`with_file` 的 `Result a IoError` 中,`a` 是回调的返回值——错误在回调结束后才存在,消费它的 case 因此圈住整个调用。值向前流的链由 `<-` 摊平(bind.md);值向外流的调用由 case 圈住。两种流向,两种形状,各一层。
