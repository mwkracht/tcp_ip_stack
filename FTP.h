#ifndef _FTP_H
#define _FTP_H

#include "UTrans.h"
#include "TCP.h"
#include <fstream>
#include <stdio.h>
#include <iostream>
#include <sys/time.h>

using namespace std;

class FTP{
	public:
		FTP();
		FTP(char *file, char *host_n, char *port_n);
		~FTP();
		int sendFile();
		int recvFile();
		int openPort();
	private:
		char *fileName, *host, *port;
		TCP *tcp;
		unsigned int fileLength;
};

#endif
