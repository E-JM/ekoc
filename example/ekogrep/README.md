# ekogrep

`ekogrep` is a grep-like line-search tool written in Eko.

## Usage

```sh
ekoc build ekogrep/ekogrep.eko -o ekogrep
./ekogrep -i -n 'error|warning' server.log
printf 'one\ntwo\n' | ./ekogrep -c 'o'     # 2
```
