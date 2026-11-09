# ekocut

`ekocut` is a field and character-position selector written in Eko.

## Usage

```sh
ekoc build ekocut/ekocut.eko -o ekocut
printf 'a,b,c\n' | ./ekocut -d, -f2      # b
printf 'abcdef\n' | ./ekocut -c2-4       # bcd
```
