#!/bin/sh

make

echo "
Jonathan Reed
Matthew Kracht
Project 2 Bandwidth Test Script
" > resultsBand.dat

echo "Test Bandwidth vs. Throughput
"
echo "Test Bandwidth vs. Throughput
" >> results.dat

for i in 100000 200000 300000 400000 500000 600000 700000 800000 900000 1000000
do 
for j in 1 2 3 4 5
do 
	echo "Run# $j Bandwidth: $i bps" >> results.dat
	echo "Run# $j Bandwidth $i bps"
	ps -e | grep dl_server
	./dl_server 15165 >> results.dat 2>&1 &
	./Router/router -B $i -C 15164:localhost:15165 > scrap.dat 2>&1 &
	./dl_client localhost 15164 input4MBit.dat > scratchClie.txt 2>&1
	ps -e | grep router | awk '{print $1;}'| xargs kill -9
done
done

exit 0

