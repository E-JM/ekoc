# ekouniq

`ekouniq` is a uniq-like adjacent-line filter written in Eko.

## Usage

```sh
ekoc build ekouniq/ekouniq.eko -o ekouniq
printf 'a\na\nb\n' | ./ekouniq -c
printf 'A\na\nB\n' | ./ekouniq -i -u
printf '1 apple\n2 apple\n' | ./ekouniq -s2 -c
```
