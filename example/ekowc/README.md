# ekowc

`ekowc` is a wc-like line, word, character, and byte counter written in Eko.

## Usage

```sh
ekoc build ekowc/ekowc.eko -o ekowc
printf 'hello  world\n' | ./ekowc
printf 'λ\n' | ./ekowc -c -m
./ekowc a.txt b.txt
```
