#!/bin/sh

make

echo "Test Window $1 vs. Throughput
"
echo "Test Window $1 vs. Throughput
" >> resultsWind.dat

for j in 1 2
do 
	ps -e | grep dl_server
	./dl_server 15165 >> resultsWind.dat 2>&1 &
	./Router/router -B 1000000 -D 50000 -C 15164:localhost:15165 > scrap.dat 2>&1 &
	./dl_client localhost 15164 input4MBit.dat > scratchClie.txt 2>&1
	ps -e | grep router | awk '{print $1;}'| xargs kill -9
done

exit 0

