/*
 * Jonathan Reed - 9051-66446
 * Matthew Kracht - 9053_25165
 * ECE - 5565
 * Project 2
 */

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
#define FR_FLAG 1
#define BUF_FLAG 1
#define CW_FLAG 1
#define RTT_FLAG 1

#ifdef DEBUG
#define PRINT_DEBUG(format, args...) printf("DEBUG(%s, %s, %d):"format, __FILE__, __FUNCTION__, __LINE__, ##args);
#else
#define PRINT_DEBUG(format, args...)
#endif

#ifdef ERROR
#define PRINT_ERROR(format, args...) printf("ERROR(%s, %s, %d):"format, __FILE__, __FUNCTION__, __LINE__, ##args);
#else
#define PRINT_ERROR(format, args...)
#endif

#define MTU 500
#define TCP_HEADER_SIZE 20

#define SLOWSTART 1
#define AIMD 2
#define FASTREC 3

#define MAX_SEND_BUFF 65535
#define MAX_RECV_BUFF 65535
#define MIN_TIMEOUT 1
#define MAX_TIMEOUT 10000

#define ACK_FLAG 0x0010
#define RST_FLAG 0x0003
#define SYN_FLAG 0x0002
#define FIN_FLAG 0x0001

class FTP;

using namespace std;

class TCP {
public:
	TCP();
	~TCP();

	int connectTCP(char *addr, char *port); //need to add three way handshake capability
	int listenTCP(char *port); //need to add three way handshake capability
	int write(char *buffer, unsigned int bufLen);
	int read(char *buffer, unsigned int bufLen, double millis = 0.0);
	int closeTCP();

	void stopTimer(timer_t timer);
	void startTimer(timer_t timer, double millis);

	int sock;
	struct addrinfo *clientAddr;
	struct addrinfo *serverAddr;

	unsigned int MSS;
	unsigned short window;
	unsigned short recvWindow;
	unsigned int congState;
	double congWindow;
	double threshhold;

	sem_t send_sem;
	char sendBuffer[MAX_SEND_BUFF];
	unsigned int bufInd;
	unsigned int bufLen;
	sem_t sendWait_sem;

	unsigned int clientSeq; //seq num rand gen by client expected by server
	unsigned int serverSeq; //seq num rand gen by server expected by client
	OrderedList *dataList;
	OrderedList *packetList;
	sem_t data_sem;
	sem_t packet_sem;

	unsigned int firstRTT;
	unsigned int seqEndRTT;
	struct timeval stampRTT;
	double estRTT;
	double devRTT;
	double timeout;
	timer_t to_timer;

private:
	pthread_t send;
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
