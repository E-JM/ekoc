# eko 时间 —— Time、Mono 与 Duration

状态:设计定案(2026-10)。结论先行,理由随文。
本文关闭 need.md 待定项"时间类型:`Clock.now` 返回型"。

## 0. 裁定一览

| # | 裁定 |
|---|---|
| 1 | 三型入 prelude:`Time`(壁钟点)、`Mono`(单调读数)、`Duration`(时长);名义单变体,载荷纳秒 `Int` |
| 2 | `Clock.now: (Clock) !-> Time`;`Clock.mono: (Clock) !-> Mono`;`Clock.sleep: (Clock, Duration) !-> _` |
| 3 | 壁钟与单调分型:壁钟可跳(校时),单调不倒退;相减各归各型,混算结构性不可能 |
| 4 | 读时与睡眠置位;时间算术全纯(值上函数,无运算符糖) |
| 5 | 派生 `Clock.frozen: (Time) -> Clock` 纯:恒读定值,sleep 即刻返回——测试确定性 |
| 6 | `Duration` 带符号;构造词 `Duration.secs/millis`;原生纳秒走构造器 |
| 7 | `Time.show` 覆写 ISO 8601 UTC;inspect 恒构造形;`Mono` 无人类形;时区/日历/parse 另立 |
| 8 | Int64 纳秒域约 ±292 年;溢出 panic(算术既有规则);闰秒不入,如实记 |

## 1. 三型

```
Time:     type = Time Int        // 壁钟点:1970-01-01T00:00:00Z 起纳秒
Mono:     type = Mono Int        // 单调读数:纪元任意,只有相对意义
Duration: type = Duration Int    // 时长:带符号,纳秒
```

时间三种量,语义互斥:时间戳(人历时刻)、测时(先后与长短)、时长
(差)。裸 `Int` 是单位谎言——毫秒微秒之争是真实事故源;名义单变体
(type.md §2)把单位封进类型,与温度事故(Celsius/Fahrenheit)同款
堵死。壁钟会跳(校时回拨),单调钟不会——两种"现在"分型,差与
比较各归各:

```
Time.sub:       (Time, Time) -> Duration
Time.add:       (Time, Duration) -> Time
Time.to_ns:     (Time) -> Int              // 桥族,Char.to_code 同款
Mono.sub:       (Mono, Mono) -> Duration
Mono.add:       (Mono, Duration) -> Mono
Mono.to_ns:     (Mono) -> Int
Duration.add:   (Duration, Duration) -> Duration
Duration.mul:   (Duration, Int) -> Duration   // 缩放;负参即反号
Duration.neg:   (Duration) -> Duration
Duration.secs:  (Int) -> Duration             // ×10⁹
Duration.millis:(Int) -> Duration             // ×10⁶
Duration.to_ns: (Duration) -> Int
```

- 全纯:值上的算术,不触世界。`==` 与 `<` 随派生(单变体载荷
  递推,derive.md §5)。
- **无运算符糖**:`t2 - t1` 不存在——单变体载荷不裸露(type.md
  §7.3 匿名 ADT 不存在),运算符够不着;一切时间算术具名。单位
  事故无处发生。
- 原生纳秒构造走构造器应用(`Duration 500`、`Time 0`,`Ok 3`
  同款);`to_ns` 是它的逆。

## 2. Clock 三服务

```
Clock.now:   (Clock) !-> Time         // 壁钟:时间戳
Clock.mono:  (Clock) !-> Mono         // 单调:测时——先后与时长
Clock.sleep: (Clock, Duration) !-> _  // 睡眠;负时长即刻返回;粒度随宿主
```

- 读时置位(io-world.md §5.1 规则 2 既有):两次读数不同,即观测
  世界。睡眠置位:时间流逝、进程让出,世界被改变;且须持
  `Clock`——位与权限同源。
- **测时用 mono,时间戳用 now**:壁钟差可负可跳,elapsed 语义只在
  单调钟上成立——分型的教学一句话。
- `Time < Time` 是**读数序**(排序可用);先后判定用 `Mono < Mono`。
  派生 `<` 如实给出两种语义,分型使误用不相遇。
- 睡眠负时长即刻返回(截断为零,如实记);超时/取消属并发裁定
  (io-world.md §8 边界),不预支词汇。

```eko
main: (out: Out, clock: Clock) ! =
    m0 = clock.mono()
    clock.sleep(Duration.millis(500))
    m1 = clock.mono()
    out.println("elapsed: {m1.sub(m0)}")       // 0.5s
    stamp = clock.now()
    out.println("at: {stamp}")                 // 2026-10-22T00:00:00Z
```

## 3. frozen:测定的派生

```
Clock.frozen: (Time) -> Clock        // 纯
```

`frozen(t)` 造一枚恒读钟:`now` 恒 `t`,`mono` 恒一固定读数,
`sleep` 即刻返回(时间不流)。与 `Out.null` 同族(io-world.md §2
纯派生),测试确定性由此来(test.md §5 收编)。

位照常:`frozen` 钟上的 `now()` 仍置位——位按内建判据静态裁定,
不看病值(位住箭头,不感染数据,io-world.md §5.1)。测试的确定性
由值保证,位的诚实由箭头保证,两不相欠。

## 4. show 与 inspect

| 型 | show | inspect |
|---|---|---|
| `Time` | `2026-10-22T01:29:00.123Z`(UTC;小数按需,至多九位) | `Time 1792632540123000000` |
| `Duration` | `0.5s`(十进秒,去尾零) | `Duration 500000000` |
| `Mono` | `Mono 89347123456`(无人类形——纪元无意义) | 同 |

- `Time.show`、`Duration.show` 是 prelude 覆写(derive.md §2 既有
  机器,声明模块即 prelude);inspect 永远构造形,所见可所写。
- UTC 唯一:本地时区是确定性敌人。格式化/本地化、`Time.parse`/
  `Duration.parse` 另立若痛——往返性质是 convert.md 对既有 parse
  族的验收,不倒逼新词。

## 5. 域与溢出

- Int64 纳秒:壁钟域约 1677-09-21 至 2262-04-11,`Duration` 同幅;
  越域算术 panic(expr.md §2 既有)。日历工程提前百年另立,如实记。
- 闰秒不入:POSIX 纪元直映,一日恒 86400 秒;对齐策略属宿主域,
  如实记。

## 6. 禁止清单

- **壁钟单调合一型**——Go `Time` 双读数暗坑;两读数两型,差各归各。
- **时间运算符糖**——载荷不裸露;具名算术是单位安全的代价与凭据。
- **本地时区/日历族(Date/DateTime)**——确定性优先;另立若痛。
- **`Clock` 派生开构造权**——frozen 是纯组合子,不是造钟自由
  (能力不透明,io-world.md §2.1);`Mono` 直构同理不设
  `Clock.frozen_mono` 一类,YAGNI。

## 7. 速查

```
clock.now()                    // Time;时间戳
clock.mono()                   // Mono;测时
m1.sub(m0)                     // Duration;单调差,测时唯一可信
t1.sub(t0)                     // Duration;壁钟差,可负可跳
clock.sleep(Duration.millis(500))
stamp.add(Duration.secs(90))   // Time + Duration
fix = Clock.frozen(Time 0)
fix.now()                      // 仍置位;值恒 Time 0
"{stamp}"                      // 2026-10-22T01:29:00.123Z
inspect(stamp)                 // Time 1792632540123000000
```
