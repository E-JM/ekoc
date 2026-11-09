# ekodiff

`ekodiff` is a unified-diff generator and patch applier written in Eko.

## Usage

```sh
ekoc build ekodiff/ekodiff.eko -o ekodiff
./ekodiff old.txt new.txt > change.patch   # generate
./ekodiff --apply change.patch             # apply
```
