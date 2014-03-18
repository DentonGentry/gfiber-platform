#!/bin/sh
FILE=$1
if [ -z "$FILE" ]; then
  echo "usage: $0 <filename>" >&2
  exit 1
fi

printf 'START ' >$FILE

# log SIGTERM and exit
trap "printf 'TERM ' >>$FILE; exit 0" TERM

# log SIGHUP but *don't* exit
trap "printf 'HUP ' >>$FILE" HUP

while sleep 0.1; do
  echo "PPID=$PPID"
  [ "$PPID" -gt 1 ] && kill -0 "$PPID" || exit 2
done
