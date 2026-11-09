# ekotee

`ekotee` is a tee-like pipe fitting written in Eko: it copies standard input to standard output and to each named file.

## Usage

```sh
ekoc build ekotee/ekotee.eko -o ekotee
printf 'a\nb\n' | ./ekotee copy.txt
printf 'more\n' | ./ekotee -a copy.txt
```
