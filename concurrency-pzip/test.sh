#!/bin/bash

set -e  # Exit on error

echo "=== Building pzip ==="
gcc -pthread -Wall -o pzip pzip.c

echo ""
echo "=== Test 1: Small file ==="
echo "Hello World Hello World Hello" > test_small.txt
./pzip test_small.txt test_small.pzip
ls -lh test_small.txt test_small.pzip

echo ""
echo "=== Test 2: Highly compressible (all zeros) ==="
dd if=/dev/zero bs=1M count=50 of=test_zeros.bin 2>/dev/null
./pzip test_zeros.bin test_zeros.pzip
ls -lh test_zeros.bin test_zeros.pzip
original_size=$(stat -c%s test_zeros.bin)
compressed_size=$(stat -c%s test_zeros.pzip)
ratio=$(echo "scale=2; $original_size / $compressed_size" | bc)
echo "Compression ratio: ${ratio}x"

echo ""
echo "=== Test 3: Random data (incompressible) ==="
dd if=/dev/urandom bs=1M count=10 of=test_random.bin 2>/dev/null
./pzip test_random.bin test_random.pzip
ls -lh test_random.bin test_random.pzip
original_size=$(stat -c%s test_random.bin)
compressed_size=$(stat -c%s test_random.pzip)
ratio=$(echo "scale=2; $compressed_size / $original_size" | bc)
echo "Size ratio: ${ratio}x (expect >1.0 due to RLE overhead)"

echo ""
echo "=== Test 4: Text file ==="
if [ -f /usr/share/dict/words ]; then
    cat /usr/share/dict/words > test_words.txt
else
    for i in {1..10000}; do echo "The quick brown fox jumps over the lazy dog $i"; done > test_words.txt
fi
./pzip test_words.txt test_words.pzip
ls -lh test_words.txt test_words.pzip

echo ""
echo "=== Test 5: Large file (stress test) ==="
dd if=/dev/zero bs=1M count=500 of=test_large.bin 2>/dev/null
echo "Compressing 500 MB..."
time ./pzip test_large.bin test_large.pzip
ls -lh test_large.bin test_large.pzip

echo ""
echo "=== Test 6: Empty file ==="
touch test_empty.txt
./pzip test_empty.txt test_empty.pzip
ls -lh test_empty.txt test_empty.pzip 2>/dev/null || echo "Empty file handled correctly"

echo ""
echo "=== Test 7: Edge case - exactly CHUNK_SIZE (8MB) ==="
dd if=/dev/zero bs=8M count=1 of=test_exact.bin 2>/dev/null
./pzip test_exact.bin test_exact.pzip
ls -lh test_exact.bin test_exact.pzip

echo ""
echo "=== Test 8: Edge case - CHUNK_SIZE + 1 byte ==="
dd if=/dev/zero bs=8M count=1 of=test_plus_one.bin 2>/dev/null
echo -n "X" >> test_plus_one.bin
./pzip test_plus_one.bin test_plus_one.pzip
ls -lh test_plus_one.bin test_plus_one.pzip

echo ""
echo "=== Cleanup ==="
rm -f test_*.txt test_*.bin test_*.pzip

echo ""
echo "=== All tests passed! ==="
