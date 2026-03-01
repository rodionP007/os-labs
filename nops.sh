#!/bin/bash
SRC="big.bin"
DST="copy.bin"
BS=1048576

for n in 1 2 4 8 12 16; do
  rm -f "$DST"
  ./aio_copy "$SRC" "$DST" "$n" "$BS"
  cmp -s "$SRC" "$DST" && echo "n_ops=$n OK" || echo "n_ops=$n DIFF"
done
