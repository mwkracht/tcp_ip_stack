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

#define MTU 500

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
		struct addrinfo *ClientAddr;
		struct addrinfo *ServerAddr;
		int SendBase;
		int WindowSize;
	private:
		pthread_t recv;
};

struct TCP_hdr {
	unsigned short SrcPort;
	unsigned short DstPort;
	unsigned int SeqNum;
	unsigned int Ack;
	unsigned short flags;
	unsigned short window;
	unsigned short checksum;
	unsigned short urgent;
	unsigned char *options;
}

#endif
