# ekosort

`ekosort` is a sort utility written in Eko.

## Usage

```sh
ekoc build ekosort/ekosort.eko -o ekosort
printf 'b10\nb9\n' | ./ekosort -n
printf 'apple 5\napple 2\n' | ./ekosort -k1,1 -k2,2n
./ekosort -rn access.log
```
