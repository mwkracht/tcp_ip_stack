all: dl_client dl_server

OrderedList.o: OrderedList.cpp OrderedList.h
	g++ -c OrderedList.cpp
TCP.o: TCP.cpp TCP.h OrderedList.o
	g++ -pthread -lrt -c TCP.cpp
UTrans.o: UTrans.cpp UTrans.h
	g++ -c UTrans.cpp
FTP.o: FTP.cpp FTP.h UTrans.o TCP.o
	g++ -c FTP.cpp

dl_client.o: dl_client.cpp FTP.o
	g++ -c dl_client.cpp
dl_client: dl_client.o FTP.o UTrans.o OrderedList.o
	g++ -pthread -lrt -o dl_client dl_client.o FTP.o UTrans.o TCP.o OrderedList.o

dl_server.o: dl_server.cpp FTP.o
	g++ -c dl_server.cpp
dl_server: dl_server.o FTP.o UTrans.o OrderedList.o
	g++ -pthread -lrt -o dl_server dl_server.o FTP.o UTrans.o TCP.o OrderedList.o	

clean: 
	rm -f *.o dl_client dl_server
