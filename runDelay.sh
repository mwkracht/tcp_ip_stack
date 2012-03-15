#!/bin/sh

#make

echo "
Jonathan Reed
Matthew Kracht
Project 2 Delay Test Script
" > resultsDelay.dat

echo "Test Delay vs. Throughput
"
echo "Test Delay vs. Throughput
" >> resultsDelay.dat

for i in 0 50000 75000 100000 150000 200000 250000 300000 400000 500000 600000
do 
for j in 1 2 3
do 
	echo "Run# $j Delay: $i us" >> resultsDelay.dat
	echo "Run# $j Delay $i us"
	ps -e | grep dl_server
	./dl_server 15165 >> results.dat 2>&1 &
	./Router/router -B 1000000 -D $i -C 15164:localhost:15165 > scrap.dat 2>&1 &
	./dl_client localhost 15164 input4MBit.dat > scratchClie.txt 2>&1
	ps -e | grep router | awk '{print $1;}'| xargs kill -9
done
done 

exit 0

