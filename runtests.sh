#!/bin/sh

EMACS=${EMACS:-emacs}


exitcode=0
for testfile in test*.el; do
    $EMACS \
              -batch \
              -l ert \
              -l "$testfile" \
              -f ert-run-tests-batch-and-exit
    if [ $? -ne 0 ]; then
       exitcode=1
    fi
done

if [ $exitcode -ne 0 ]; then
    echo "Not all tests passed"
    exit 1
fi
