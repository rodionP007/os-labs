#!/bin/bash
SRC="big.bin"
DST="copy.bin"
NOPS=2

for bs in 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304; do
  rm -f "$DST"
  ./aio_copy "$SRC" "$DST" "$NOPS" "$bs"
  cmp -s "$SRC" "$DST" && echo "bs=$bs OK" || echo "bs=$bs DIFF"
done
