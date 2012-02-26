#include "TCP.h"

short checksum(char *data, int datalen) {
	short *buf;
	int len = datalen / 2;
	int rem = datalen % 2;
	short sum = 0;	

	buf = (short *)data;
	
	for (int i = 0; i < len; i++){
		if (i == 8) {
			continue;
		}
		sum += buf[i];
		if (buf[i] < 0) {
			sum++;			
		}
	}
	
	unsigned short temp = 0;
	if (rem) {
		temp = (short)(data[datalen-1]<<8);
		if (temp < 0) {
			sum += temp+1;	
		} else {
			sum += temp;
		}
	}

	return sum;
}


void *RecvClient(void *local){
	TCP *myTCP = (TCP *)local;

	
		
}

void *RecvServer(void *local){
	TCP *myTCP = (TCP *)local;
	char *buffer;
	bool newBuf = true;
	int size;
	struct TCP_hdr *header;

	OrderedList PacketList = new OrderedList();

	while (1) {
		if (newBuf) {
			buffer = new char[MTU];
		}

		size = recv(myTCP->sock, (void *)buffer, MTU, 0);
		if(size == -1){
			printf("ERROR receiving data over socket: %s\n", strerror(errno));
			continue;
		}
		header = (struct TCP_hdr *)buffer;
		short calc = checksum(buffer, size);
		if(calc != header->checksum){
			//NACK send Base
		} else {
			if (myTCP->clientSeq == header->SeqNum) {
				//process flags
				//save to data list
				if (sem_wait(&data_sem)) {
				    perror("sem_wait prod");
				    exit(-1); //watch/change?
				}
				if (WindowSize > 0) {
					unsigned int offset = header->flags>>12;
					
					char *data = (char *)header + offset;
					int dataLen = size-offset;

					myTCP->DataList.insert(header->SeqNum, data, dataLen);
					WindowSize -= dataLen;
					
					myTCP->clientSeq++;
											
					while () {
					}
				}

				sem_post(&data_sem);
				newBuf = false;
			} else if (myTCP->clientSeq < header->SeqNum) {
				if (sem_wait(&data_sem)) {
				    perror("sem_wait prod");
				    exit(-1); //watch/change?
				}
				if (WindowSize > 0) {
					int ret = PacketList.insert(header->SeqNum, buffer, size);
					if (ret) {

					}					
					newBuf = (ret==0);
				} else {
					newBuf = false;
				}
				sem_post(&data_sem);				
			}
		}


	}		
}


TCP::TCP(){
}

TCP::~TCP(){
	free(ClientAddr);
	free(ServerAddr);
}

int TCP::connectTCP(char *addr, char *port){

	struct addrinfo HINTS, *AddrInfo, *v4AddrInfo, *v6AddrInfo;
	memset(&HINTS, 0, sizeof(HINTS));
	HINTS.ai_family = AF_UNSPEC; //search both IPv4 and IPv6 addrs
	HINTS.ai_socktype = SOCK_DGRAM; //Use UDP
	HINTS.ai_protocol = 17;
	int value = getaddrinfo(addr, port, &HINTS, &AddrInfo);
	if(value != 0) {
		cout << "Didn't resolve to addr info\n";
		return -1;
	}
	v4AddrInfo =  NULL;
	v6AddrInfo = NULL;

	while(AddrInfo != NULL){
		if(AddrInfo->ai_family == AF_INET6){
			v6AddrInfo = AddrInfo;
		} else if(AddrInfo->ai_family == AF_INET) {
			v4AddrInfo = AddrInfo;
		} else {
			printf( "Server port binding returned a non v4 or v6 Addr\n");
		}
		AddrInfo = AddrInfo->ai_next;
	}
	
	if(v6AddrInfo != NULL){
		sock = socket(v6AddrInfo->ai_family, v6AddrInfo->ai_socktype, v6AddrInfo->ai_protocol);
		if(sock != -1){
			if(connect(sock, v6AddrInfo->ai_addr, v6AddrInfo->ai_addrlen) != 0){
				printf( "Error in binding socket: %s\n", strerror( errno ) );
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf( "Error creating socket: %s\n", strerror( errno ) );
		}
		memcpy(ServerAddr, v6AddrInfo, sizeof(addrinfo));
	} else if(v4AddrInfo != NULL){
		sock = socket(v4AddrInfo->ai_family, v4AddrInfo->ai_socktype, v4AddrInfo->ai_protocol);
		if(sock != -1){
			if(connect(sock, v4AddrInfo->ai_addr, v4AddrInfo->ai_addrlen) != 0){
				printf( "Error in binding socket: %s\n", strerror( errno ) );
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf( "Error creating socket: %s\n", strerror( errno ) );
		}
		memcpy(ServerAddr, v4AddrInfo, sizeof(addrinfo));
	} else {
		printf("ERROR: Unable to find any local IP Address\n");
	}	
	freeaddrinfo(AddrInfo);

	//TODO: add in 3 way handshake & block until done
	
	clientSeq = 0;	//rand gen
	serverSeq = 0;	//sent to us
	WindowSize = MAX_RECV_BUFF; //sent to us

	if(pthread_create(&recv, NULL, RecvClient, (void *)this)){
		printf("ERROR: unable to create producer thread.\n");
		exit(-1);
	}
	return 0;
}

int TCP::listenTCP(char *port){
	struct addrinfo HINTS, *AddrInfo, *v4AddrInfo, *v6AddrInfo;
	memset(&HINTS, 0, sizeof(HINTS));
	HINTS.ai_family = AF_UNSPEC; //search both IPv4 and IPv6 addrs
	HINTS.ai_socktype = SOCK_DGRAM; //Use UDP
	HINTS.ai_protocol = 17;
	HINTS.ai_flags = AI_PASSIVE; //use wildcard to find local addrs
	int value = getaddrinfo(NULL, port, &HINTS, &AddrInfo);
	if(value != 0) {
		cout << "Didn't resolve to addr info\n";
		return -1;
	}
	v4AddrInfo = NULL;
	v6AddrInfo = NULL;

	while(AddrInfo != NULL){
		if(AddrInfo->ai_family == AF_INET6){
			v6AddrInfo = AddrInfo;
		} else if(AddrInfo->ai_family == AF_INET) {
			v4AddrInfo = AddrInfo;
		} else {
			printf( "Server port binding returned a non v4 or v6 Addr\n");
		}
		AddrInfo = AddrInfo->ai_next;
	}
	
	if(v6AddrInfo != NULL){
		sock = socket(v6AddrInfo->ai_family, v6AddrInfo->ai_socktype, v6AddrInfo->ai_protocol);
		if(sock != -1){
			if(bind(sock, v6AddrInfo->ai_addr, v6AddrInfo->ai_addrlen) != 0){
				printf( "Error in binding socket: %s\n", strerror( errno ) );
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf( "Error creating socket: %s\n", strerror( errno ) );
		}
		memcpy(ServerAddr, v6AddrInfo, sizeof(addrinfo));
	} else if(v4AddrInfo != NULL){
		sock = socket(v4AddrInfo->ai_family, v4AddrInfo->ai_socktype, v4AddrInfo->ai_protocol);
		if(sock != -1){
			if(bind(sock, v4AddrInfo->ai_addr, v4AddrInfo->ai_addrlen) != 0){
				printf( "Error in binding socket: %s\n", strerror( errno ) );
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf( "Error creating socket: %s\n", strerror( errno ) );
		}
		memcpy(ServerAddr, v4AddrInfo, sizeof(addrinfo));
	} else {
		printf("ERROR: Unable to find any local IP Address\n");
	}	
	freeaddrinfo(AddrInfo);

	//TODO: add in 3 way handshake & block until done

	serverSeq = 0;	//rand gen
	clientSeq = 0;	//sent to us
	WindowSize = MAX_RECV_BUFF;

	DataList = new OrderedList();
	sem_init(&data_sem, 0, 1);

	if(pthread_create(&recv, NULL, RecvServer, (void *)this)){
		printf("ERROR: unable to create producer thread.\n");
		exit(-1);
	}

	return 0;
}
