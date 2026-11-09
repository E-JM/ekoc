# ekosql

`ekosql` is a small SQL engine over one CSV file, written in Eko.

## Usage

```sh
ekoc build ekosql/ekosql.eko -o ekosql
./ekosql "SELECT city, COUNT(*) AS n, AVG(age) FROM data.csv GROUP BY city ORDER BY n DESC"
./ekosql --csv "SELECT name, age FROM 'data/sales.csv' WHERE age > 18"
```
