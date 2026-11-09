# ekosed

`ekosed` is a sed-like stream editor written in Eko.

## Usage

```sh
ekoc build ekosed/ekosed.eko -o ekosed
printf 'a,b\n' | ./ekosed 's/,/\t/'                # a	b
printf 'john smith\n' | ./ekosed 's/([a-z]+) ([a-z]+)/\2 \1/'
printf 'x\ny\nz\n' | ./ekosed '/y/d'
```
