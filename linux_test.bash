bs=1048576
for n in 1 2 4 8 12 16; do
	./mainLinux big.bin copy.bin $n $bs
	cmp -s big.bin copy.bin && echo "bs=$bs OK" || echo "bs = $bs DIFF"
done

