# ekojq

`ekojq` is a jq-like JSON filter written in Eko.

## Usage

```sh
ekoc build ekojq/ekojq.eko -o ekojq
echo '{"a": [1, 2]}' | ./ekojq -c '.a | add'   # 3
./ekojq -n '[range(1; 4)]'                      # [1,2,3]
```
