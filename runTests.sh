#!/bin/sh

#echo "
#Jonathan Reed
#Matthew Kracht
#Project 2 Test Script
#" > results.dat

#echo "Test Bandwidth vs. Throughput
#"

#for i in 100 200 300 400 500 600 700 800 900 1000
#do 
#for j in 1 2 3 4 5
#do 
#	echo "Run# $j:" >> results.dat
#	./dl_server 15165 >> results.dat &
#	./Router/router -B $i -C 15164:localhost:15165 > scap.dat &
#	./dl_client localhost 15164 input4MBit.dat
#	ps -e | grep router | awk '{print $1;}'| xargs kill -9
#done
#done
make

for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do 
	echo "Run# $i"
	./dl_server 15165 > scratchServ.txt 2>&1 &
	./dl_client localhost 15165 input4MBit.dat > scratchClie.txt 2>&1
	sleep 1
	#ps -aux | grep ./dl_
done

exit 0

