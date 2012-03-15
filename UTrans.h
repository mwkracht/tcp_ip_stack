/*
 * Jonathan Reed - 9051-66446
 * Matthew Kracht - 9053_25165
 * ECE - 5565
 * Project 2
 */

#ifndef _UTRANS_H
#define _UTRANS_H

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

#define MTU 500

class FTP;

using namespace std;

class UTrans{
	public:
		UTrans();
		UTrans(char *IP, char *port_num, FTP* par); //constructor for send port (udp)
		UTrans(char *port_num, FTP* par); //constructor for recv port (udp)
		~UTrans();
		int transmit(char *buffer, unsigned int buf_size);
		int receive(char *buffer, unsigned int buf_size);
		bool isEstablished();
	private:
		int sock;
		bool isSend;
		struct addrinfo *info;
		bool established;
		FTP *parent;
};

#endif
