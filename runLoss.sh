#!/bin/sh

#make

echo "
Jonathan Reed
Matthew Kracht
Project 2 Loss Test Script
" > resultsLoss.dat

echo "Test Loss vs. Throughput
"
echo "Test Loss vs. Throughput
" >> resultsLoss.dat

for i in 10 20 40 60 80 100 150 200 250 300
do 
for j in 1 2 3
do 
	echo "Run# $j Loss: $i bps" >> resultsLoss.dat
	echo "Run# $j Loss $i bps"
	ps -e | grep dl_server
	./dl_server 15165 >> resultsLoss.dat 2>&1 &
	./Router/router -B 1000000 -D 0 -L $i -C 15164:localhost:15165 > scrap.dat 2>&1 &
	./dl_client localhost 15164 input8MBit.dat > scratchClie.txt 2>&1
	ps -e | grep router | awk '{print $1;}'| xargs kill -9
done
done

exit 0


