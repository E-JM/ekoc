# syslog

`syslog` is a syslog-style logger and log viewer written in Eko.

## Usage

```sh
ekoc build syslog/syslog.eko -o syslog
./syslog write -t backupd "disk nearly full"
printf 'line one\nline two\n' | ./syslog write -t feeder
./syslog show -l err -t backupd --raw
```
