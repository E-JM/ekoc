# resources

`NotoSansSC-demo.ttf` — a deliberately tiny subset of **Noto Sans SC**
(Google Fonts, SIL Open Font License 1.1), holding exactly the 125 codepoints
this demo draws: ASCII 32–126 plus every character appearing in `msg1`, `msg2`,
and `msg3` in `../main.eko`.

raylib rasterizes TrueType outlines with stb_truetype, which cannot read the
system's CFF-based Noto Sans CJK `.ttc` collections, so a TTF is shipped here
instead. It is a subset because the full variable font is ~17 MB; the demo
needs ~16 KB.

Regenerate after changing the messages (needs `fonttools`):

```sh
curl -sL -o /tmp/NotoSansSC.ttf \
  "https://cdn.jsdelivr.net/gh/google/fonts@main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf"

python3 - <<'PY'
import re
src = open('main.eko', encoding='utf-8').read()
msgs = re.findall(r'^msg\d: String = "(.*)"$', src, re.M)
chars = sorted({c for m in msgs for c in m} | {chr(c) for c in range(32, 127)})
open('/tmp/chars.txt', 'w', encoding='utf-8').write(''.join(chars))
PY

pyftsubset /tmp/NotoSansSC.ttf --text-file=/tmp/chars.txt \
  --output-file=resources/NotoSansSC-demo.ttf \
  --drop-tables+=gvar,STAT,fvar,avar,HVAR --no-hinting --layout-features=''
```

The font is loaded with an explicit codepoint list by `../font_shim.c`, so a
glyph missing from the subset renders as raylib's fallback `?` instead of
aborting.
