# ekotr

`ekotr` is a tr-like character translator, squeezer, and deleter written in Eko.

## Usage

```sh
ekoc build ekotr/ekotr.eko -o ekotr
printf 'hello\n' | ./ekotr 'a-z' 'A-Z'
printf 'aabbcc\n' | ./ekotr -ds a b
printf 'aaa bbb\n' | ./ekotr -s ab
```
