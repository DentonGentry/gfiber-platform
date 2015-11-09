exit=0
run=.run.$$
for n in tests.mmap/*.test; do
  rm -rf $run
  mkdir $run
  : > $run/LOG
  (
    cd $run
    . ../$n
    echo -n "$INPUT" > INPUT
    echo -n "$OUTPUT" > WANT
    for m in $FILES; do
      eval IN="\$${m}_1"
      eval OUT="\$${m}_2"
      echo -n "$IN" > $m
      echo -n "$OUT" > $m.expected
    done
    $PREFIX ../host-mmap < INPUT >& GOT
    status=$?
    if [ -n "$PREFIX" ]; then
      sleep .5      # script mysteriously delays output
    fi
    if [ "$status" != "$EXIT" ]; then
      echo "exit code: expected '$EXIT', got '$status'"
    fi
    if ! cmp WANT GOT; then
      echo "output differs from expected:"
      diff -u WANT GOT
    fi
    for m in $FILES; do
      if ! cmp $m $m.expected; then
        echo "mapped file result differs from expected:"
        diff -u $m $m.expected
      fi
    done
  ) >& $run/LOG
  if [ ! -s $run/LOG ]; then
    echo PASS $n
  else
    echo ======================================================================
    echo FAIL $n
    cat $run/LOG
    echo ======================================================================
    exit=1
  fi
done
rm -rf $run
exit $exit
