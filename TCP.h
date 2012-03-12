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
#include <time.h>
#include <signal.h>
#include "OrderedList.h"

#define MTU 492
#define MAX_RECV_BUFF 10240
#define TCP_HEADER_SIZE 20

#define ACK_FLAG 0x0010
#define RST_FLAG 0x0003
#define SYN_FLAG 0x0002
#define FIN_FLAG 0x0001

#define CONTINUE 1
#define GBN 2

#define SLOWSTART 1
#define AIMD 2
#define FASTREC 3

class FTP;

using namespace std;

class TCP{
	public:
		TCP();
		~TCP();
		int connectTCP(char *addr, char *port);
		int listenTCP(char *port); //need to add three way handshake capability
		int write(char *buffer, unsigned int bufLen);
		int read(char *buffer, unsigned int bufLen, int millis=0);
		//int closeTCP();
		unsigned int setTimeoutTimer(timer_t timer, int millis);
		int sock;
		struct addrinfo *clientAddr;
		struct addrinfo *serverAddr;
		unsigned int window;
		unsigned int recvWindow;
		unsigned int congWindow;
		unsigned int congState;
		unsigned int MSS;
		unsigned int threshhold;
		char recvBuffer[MAX_RECV_BUFF];
		unsigned int clientSeq; //seq num rand gen by client expected by server
		unsigned int serverSeq;	//seq num rand gen by server expected by client
		unsigned int sendBase;
		sem_t data_sem;
		sem_t packet_sem;
		OrderedList *dataList;
		OrderedList *packetList;
		timer_t to_timer;
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
	unsigned char options;
};

#endif
