#ifndef _TCP_H
#define _TCP_H

#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "OrderedList.h"

#define MTU 500
#define MAX_RECV_BUFF 10240

class FTP;

using namespace std;

class TCP{
	public:
		TCP();
		//UTrans(char *IP, char *port_num, FTP* par); //constructor for send port (udp)
		//UTrans(char *port_num, FTP* par); //constructor for recv port (udp)
		~TCP();
		int connectTCP(char *addr, char *port);
		int listenTCP(char *port); //need to add three way handshake capability
		//int transmit(char *buffer, unsigned int buf_size);
		//int receive(char *buffer, unsigned int buf_size);
		//int closeTCP();
		//bool isEstablished();
		int sock;
		struct addrinfo *clientAddr;
		struct addrinfo *serverAddr;
		int base;
		int windowSize;
		char recvBuffer[MAX_RECV_BUFF];
		unsigned int clientSeq; //seq num rand gen by client expected by server
		unsigned int serverSeq;	//seq num rand gen by server expected by client
		sem_t data_sem;
		OrderedList *dataList;
	private:
		pthread_t recv;
};

struct TCP_hdr {
	unsigned short srcPort;
	unsigned short dstPort;
	unsigned int seqNum;
	unsigned int ack;
	unsigned short flags;
	unsigned short window;
	unsigned short checksum;
	unsigned short urgent;
	unsigned char *options;
};

#endif
