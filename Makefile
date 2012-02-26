all: dl_client.o FTP.o UTrans.o OrderedList.o
	g++ -pthread -o dl_client dl_client.o FTP.o UTrans.o TCP.o OrderedList.o
dl_client.o: dl_client.cpp FTP.o
	g++ -c dl_client.cpp
FTP.o: FTP.cpp FTP.h UTrans.o TCP.o
	g++ -c FTP.cpp
UTrans.o: UTrans.cpp UTrans.h
	g++ -c UTrans.cpp
TCP.o: TCP.cpp TCP.h OrderedList.o
	g++ -pthread -c TCP.cpp
OrderedList.o: OrderedList.cpp OrderedList.h
	g++ -c OrderedList.cpp

clean: 
	rm -f *.o dl_client
