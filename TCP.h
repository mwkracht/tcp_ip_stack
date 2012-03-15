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
#include <cmath>
#include <math.h>
#include <sys/time.h>

//#define DEBUG
//#define ERROR

#ifdef DEBUG
#define PRINT_DEBUG(format, args...) printf("DEBUG(%s, %d):"format, __FILE__, __LINE__, ##args);
#else
#define PRINT_DEBUG
#endif

#ifdef ERROR
#define PRINT_ERROR(format, args...) printf("ERROR(%s, %d):"format, __FILE__, __LINE__, ##args);
#else
#define PRINT_ERROR
#endif

#define MTU 500
#define MAX_RECV_BUFF 480*1
//#define MAX_RECV_BUFF 65535
#define TCP_HEADER_SIZE 20
#define MIN_TIMEOUT 1
#define MAX_TIMEOUT 10000

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
		int read(char *buffer, unsigned int bufLen, double millis=0.0);
		//int closeTCP();
		double stopTimer(timer_t timer);
		void startTimer(timer_t timer, double millis);
		unsigned int setTimer(timer_t timer, int millis); //deprecated
		int sock;
		struct addrinfo *clientAddr;
		struct addrinfo *serverAddr;
		unsigned short window;
		unsigned short recvWindow;
		double congWindow;
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
		double timeout;
		double estRTT;
		double devRTT;
		unsigned int validRTT;
		unsigned int seqEndRTT;
		struct timeval stampRTT;
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
