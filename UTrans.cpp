#include "UTrans.h"

UTrans::UTrans(){

}

UTrans::UTrans(char *host, char *port_num, FTP* par){

	established = false;
	struct addrinfo HINTS, *AddrInfo;
	memset(&HINTS, 0, sizeof(HINTS));
	HINTS.ai_family = AF_UNSPEC; //search both IPv4 and IPv6 addrs
	HINTS.ai_socktype = SOCK_DGRAM; //Use UDP
	HINTS.ai_protocol = 17;
	int value = getaddrinfo(host, port_num, &HINTS, &AddrInfo);
	if(value != 0) {
		cout << "Didn't resolve to addr info\n";
		return;
	}
	info = AddrInfo;

	while(AddrInfo != NULL){
		sock = socket(AddrInfo->ai_family, AddrInfo->ai_socktype, AddrInfo->ai_protocol);
		if(sock != -1){//Try to connect to socket if it is not invalid
			if(connect(sock, AddrInfo->ai_addr, AddrInfo->ai_addrlen) == 0){
				established = true;
				break;
			} else {
				printf( "Error in connecting socket: %s\n", strerror( errno ) );
			}
		} else {
			printf( "Error creating socket: %s\n", strerror( errno ) );
		}
		close(sock);
		AddrInfo = AddrInfo->ai_next;
	}
	parent = par;
	isSend = true;
}

UTrans::UTrans(char *port_num, FTP* par){
	established = false;
	struct addrinfo HINTS, *AddrInfo, *v4AddrInfo, *v6AddrInfo;
	memset(&HINTS, 0, sizeof(HINTS));
	HINTS.ai_family = AF_UNSPEC; //search both IPv4 and IPv6 addrs
	HINTS.ai_socktype = SOCK_DGRAM; //Use UDP
	HINTS.ai_protocol = 17;
	HINTS.ai_flags = AI_PASSIVE; //use wildcard to find local addrs
	int value = getaddrinfo(NULL, port_num, &HINTS, &AddrInfo);
	if(value != 0) {
		cout << "Didn't resolve to addr info\n";
		return;
	}
	info = AddrInfo;
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
			if(bind(sock, v6AddrInfo->ai_addr, v6AddrInfo->ai_addrlen) == 0){
				established = true;
			} else {
				printf( "Error in binding socket: %s\n", strerror( errno ) );
				close(sock);
			}
		} else {
			printf( "Error creating socket: %s\n", strerror( errno ) );
		}
	} else if(v4AddrInfo != NULL){
		sock = socket(v4AddrInfo->ai_family, v4AddrInfo->ai_socktype, v4AddrInfo->ai_protocol);
		if(sock != -1){
			if(bind(sock, v4AddrInfo->ai_addr, v4AddrInfo->ai_addrlen) == 0){
				established = true;
			} else {
				printf( "Error in binding socket: %s\n", strerror( errno ) );
				close(sock);
			}
		} else {
			printf( "Error creating socket: %s\n", strerror( errno ) );
		}
	} else {
		printf("ERROR: Unable to find any local IP Address\n");
	}

	parent = par;
	isSend = false;
}

UTrans::~UTrans(){
	close(sock);
	freeaddrinfo(info);
}

int UTrans::transmit(char *buffer, unsigned int buf_size){
	if(!isSend){
		printf("ERROR: Attempting to transmit using a recv socket.\n");
		return -1;
	}

	for(int i=0;i<buf_size;i=i+MTU){
		if((buf_size-i)<MTU){ //send fragment
			int temp = buf_size-i;
			if(send(sock,(void *)(buffer+(i)),temp,0) == -1){
				printf( "Error sending data over socket: %s\n", strerror( errno ) );
				return -1;
			}
		} else { //send MTU
			if(send(sock,(void *)(buffer+(i)),MTU,0) == -1){
				printf( "Error sending data over socket: %s\n", strerror( errno ) );
				return -1;
			}
		}
		usleep(100);	
	}
	return 0;
}

bool UTrans::isEstablished(){
	return established;
}

int UTrans::receive(char *buffer, unsigned int buf_size){
	if(isSend){
		printf("ERROR: Attemping to receive message while configured to send.\n");
		return -1;
	}

	for(int i=0;i<buf_size;i = i + MTU){
		printf("r%d|",i);
		if((buf_size-i)<MTU){ //recv fragment
			int temp = buf_size-i;
			if(recv(sock,(void *)(buffer+(i)),temp,0) == -1){
				printf("ERROR receiving data over socket: %s\n", strerror(errno));
			return -1;
			}
		} else { //recv MTU
			if(recv(sock,(void *)(buffer+(i)),MTU,0) == -1){
				printf("ERROR receiving data over socket: %s\n", strerror(errno));
				return -1;
			}
		}
	}
	printf("Broke loop..\n");

	return 0;
}
