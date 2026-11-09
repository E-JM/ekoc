# ekogit

`ekogit` is a toy content-addressed version-control system written in Eko.

## Usage

```sh
ekoc build ekogit/ekogit.eko -o ekogit
mkdir repo && cd repo
./ekogit init
./ekogit add -A .
./ekogit commit -m "first commit"
./ekogit status
```
