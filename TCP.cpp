#include "TCP.h"

int timeoutFlag;
int waitFlag;
sem_t write_sem;

void TCP::setTimeoutTimer(timer_t timer, int millis) {
	struct itimerspec its;
	its.it_value.tv_sec = 0;
	its.it_value.tv_nsec = millis * 1000000;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;

	if (timer_settime(timer, 0, &its, NULL) == -1) {
		printf("Error setting timer.\n");
		exit(-1);
	}
}

static void to_handler(int sig, siginfo_t *si, void *uc) {
	cout << "acquired and posted\n";
	timeoutFlag = 1;
	if (waitFlag == 1) {
		sem_post(&write_sem);
	}
}

unsigned short checksum(char *data, int datalen) {
	short *buf;
	int len = datalen / 2;
	int rem = datalen % 2;
	unsigned short sum = 0;

	buf = (short *) data;

	for (int i = 0; i < len; i++) {
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
		temp = (short) (data[datalen - 1] << 8);
		if (temp < 0) {
			sum += temp + 1;
		} else {
			sum += temp;
		}
	}

	return sum;
}

void *recvClient(void *local) {
	TCP *myTCP = (TCP *) local;
	char *buffer = new char[MTU];
	struct TCP_hdr *header;
	unsigned short offset;
	unsigned int dataLen;
	unsigned short calc;
	int dupSequence = 0;
	int ret;
	char *data;
	int size;
	Node *head;
	socklen_t peerLen;
	struct sockaddr_storage *peerAddr;

	while (1) {

		size = recvfrom(myTCP->sock, (void *) buffer, MTU, 0,
				(struct sockaddr *) peerAddr, &peerLen);
		if (size == -1) {
			printf("ERROR receiving data over socket: %s\n", strerror(errno));
			continue;
		}

		header = (struct TCP_hdr *) buffer;
		offset = 4 * (header->flags >> 12);
		data = (char *) header + offset;
		dataLen = size - offset;

		printf("Received packet, seq=%d, size=%d\n", header->seqNum, size);

		//check that ports match expected or reject packet
		calc = checksum(buffer, size);
		if (calc != header->checksum) {
			printf("recvChecksum:%u calcChecksum:%u\n", header->checksum, calc);
		} else {
			if (header->flags & ACK_FLAG) {

				while ((ret = sem_wait(&myTCP->packet_sem)) == -1 && errno == EINTR)
					;
				if (ret == -1 && errno != EINTR) {
					perror("sem_wait prod");
					exit(-1);
				}

				myTCP->recvWindow = header->window;
				myTCP->window = header->window;
				myTCP->serverSeq = header->seqNum;

				//reset Timer...

				head = myTCP->packetList->peekHead();
				printf("Sendbase: %u\n", head->seqNum);
				if (header->seqNum == head->seqNum) {
					dupSequence++;
					if (dupSequence == 3) {
						while ((ret = sem_wait(&myTCP->state_sem)) == -1 && errno == EINTR)
							;
						if (ret == -1 && errno != EINTR) {
							perror("sem_wait prod");
							exit(-1);
						}
						myTCP->state = FAST;
						sem_post(&myTCP->state_sem);
					}
				} else {
					while (header->seqNum > head->seqEnd) {
						myTCP->packetList->removeHead();
						head = myTCP->packetList->peekHead();
					}
				}

				sem_post(&myTCP->packet_sem);

			} else {
				printf("Error: Client received packet without ACK set.");
			}
		}

	}

}

void *recvServer(void *local) {
	TCP *myTCP = (TCP *) local;
	char *buffer;
	bool newBuf = true;
	int size;
	struct TCP_hdr *header;
	struct TCP_hdr *ackHdr;
	unsigned int offset;
	char *data;
	int dataLen;
	Node *head;
	char ackBuf[MTU];
	socklen_t peerLen;
	unsigned short calc;
	struct sockaddr_storage *peerAddr;
	int ret;

	myTCP->packetList = new OrderedList();
	peerAddr = new sockaddr_storage;
	peerLen = sizeof(struct sockaddr_storage);

	while (1) {
		if (newBuf) {
			buffer = new char[MTU];
		}

		size = recvfrom(myTCP->sock, (void *) buffer, MTU, 0,
				(struct sockaddr *) peerAddr, &peerLen);
		if (size == -1) {
			printf("ERROR receiving data over socket: %s\n", strerror(errno));
			continue;
		}

		header = (struct TCP_hdr *) buffer;
		offset = 4 * (header->flags >> 12);
		data = (char *) header + offset;
		dataLen = size - offset;

		printf("Received packet, seq=%d, size=%d\n", header->seqNum, size);

		//check that ports match expected or reject packet
		calc = checksum(buffer, size);
		if (calc != header->checksum) {
			newBuf = false;
			//NACK send base
			printf("recvChecksum:%u calcChecksum:%u\n", header->checksum, calc);
		} else {
			while ((ret = sem_wait(&myTCP->data_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}
			if (myTCP->clientSeq == header->seqNum) {
				printf("In order seq=%d\n", header->seqNum);
				//process flags

				myTCP->dataList->insert(header->seqNum, header->seqNum
						+ dataLen - 1, data, dataLen);
				myTCP->window -= dataLen;
				myTCP->clientSeq += dataLen;

				while (myTCP->packetList->getSize() != 0 && myTCP->window > 0) {
					head = myTCP->packetList->peekHead();
					if (head->seqNum < myTCP->clientSeq) {
						myTCP->packetList->removeHead();
						delete head->data;
						delete head;
					} else if (head->seqNum == myTCP->clientSeq) {
						printf("Connected to seq=%d\n", header->seqNum);
						myTCP->packetList->removeHead();
						header = (struct TCP_hdr *) head->data;

						//Process Flags

						offset = 4 * (header->flags >> 12);
						data = (char *) header + offset;
						dataLen = size - offset;

						myTCP->dataList->insert(header->seqNum, header->seqNum
								+ dataLen - 1, data, dataLen);
						myTCP->clientSeq += dataLen;
					} else {
						break;
					}
				}
				newBuf = true;
			} else {

				//TODO: handle circular seq num
				if (myTCP->clientSeq < header->seqNum && header->seqNum
						+ dataLen <= myTCP->clientSeq + MAX_RECV_BUFF) {
					printf("Buffering out of order exp=%d seq=%d\n",
							myTCP->clientSeq, header->seqNum);
					int ret = myTCP->packetList->insert(header->seqNum,
							header->seqNum + dataLen - 1, buffer, size);
					if (ret) {
						//is dubplicate / colliding
						newBuf = false;
					} else {
						myTCP->window -= dataLen;
						newBuf = true;
					}
				} else {
					printf("Dropping duplicate exp=%d seq=%d\n",
							myTCP->clientSeq, header->seqNum);
				}
			}

			//ACK
			printf("Sending ACK seq=%d\n", myTCP->clientSeq);

			header = (struct TCP_hdr *) buffer;

			unsigned char ackOffset = 5;
			int ackLen = 4 * (ackOffset) + 1;
			ackHdr = (struct TCP_hdr *) ackBuf;
			ackHdr->srcPort = header->dstPort;
			ackHdr->dstPort = header->srcPort;
			ackHdr->seqNum = myTCP->serverSeq;
			ackHdr->ack = myTCP->clientSeq;
			memset(&ackHdr->flags, 0, sizeof(short));
			ackHdr->flags |= (ackOffset) << 12; //check this?
			ackHdr->flags |= ACK_FLAG;
			ackHdr->window = myTCP->window;
			ackHdr->urgent = 0;

			//unsigned char *options;
			//data? if we have 2way msging

			sem_post(&myTCP->data_sem);

			ackHdr->checksum = checksum(ackBuf, ackLen);
			sendto(myTCP->sock, ackBuf, ackLen, 0,
					(struct sockaddr *) peerAddr, peerLen);
		}
	}
}

TCP::TCP() {
}

TCP::~TCP() {
	free(clientAddr);
	free(serverAddr);
}

int TCP::connectTCP(char *addr, char *port) {

	struct sigevent sev;
	struct sigaction sa;
	struct addrinfo HINTS, *AddrInfo, *v4AddrInfo, *v6AddrInfo;

	serverAddr = new addrinfo;
	memset(&HINTS, 0, sizeof(HINTS));
	HINTS.ai_family = AF_UNSPEC; //search both IPv4 and IPv6 addrs
	HINTS.ai_socktype = SOCK_DGRAM; //Use UDP
	HINTS.ai_protocol = 17;
	int value = getaddrinfo(addr, port, &HINTS, &AddrInfo);
	if (value != 0) {
		cout << "Didn't resolve to addr info\n";
		return -1;
	}
	v4AddrInfo = NULL;
	v6AddrInfo = NULL;

	while (AddrInfo != NULL) {
		if (AddrInfo->ai_family == AF_INET6) {
			v6AddrInfo = AddrInfo;
		} else if (AddrInfo->ai_family == AF_INET) {
			v4AddrInfo = AddrInfo;
		} else {
			printf("Server port binding returned a non v4 or v6 Addr\n");
		}
		AddrInfo = AddrInfo->ai_next;
	}
	if (v6AddrInfo != NULL) {
		sock = socket(v6AddrInfo->ai_family, v6AddrInfo->ai_socktype,
				v6AddrInfo->ai_protocol);
		if (sock != -1) {
			if (connect(sock, v6AddrInfo->ai_addr, v6AddrInfo->ai_addrlen) != 0) {
				printf("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v6AddrInfo, sizeof(addrinfo));
	} else if (v4AddrInfo != NULL) {
		sock = socket(v4AddrInfo->ai_family, v4AddrInfo->ai_socktype,
				v4AddrInfo->ai_protocol);
		if (sock != -1) {
			if (connect(sock, v4AddrInfo->ai_addr, v4AddrInfo->ai_addrlen) != 0) {
				printf("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v4AddrInfo, sizeof(addrinfo));
	} else {
		printf("ERROR: Unable to find any local IP Address\n");
	}
	freeaddrinfo(AddrInfo);

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = to_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
		printf("Error creating time out timer. Exiting.\n");
		exit(-1);
	}

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_value.sival_ptr = &to_timer;
	if (timer_create(CLOCK_REALTIME, &sev, &to_timer) == -1) {
		printf("Error creating time out timer. Exiting.\n");
		exit(-1);
	}
	setTimeoutTimer(to_timer, 0);

	//TODO: add in 3 way handshake & block until done

	clientSeq = 1; //rand gen
	serverSeq = 1; //sent to us
	recvWindow = MAX_RECV_BUFF;
	window = recvWindow; //sent to us

	packetList = new OrderedList();
	sem_init(&packet_sem, 0, 1);
	sem_init(&write_sem, 0, 0);
	sem_init(&state_sem, 0, 1);

	state = CONTINUE;
	waitFlag = 0;
	timeoutFlag = 0;

	if (pthread_create(&recv, NULL, recvClient, (void *) this)) {
		printf("ERROR: unable to create producer thread.\n");
		exit(-1);
	}

	return 0;
}

int TCP::listenTCP(char *port) {
	struct addrinfo HINTS, *AddrInfo, *v4AddrInfo, *v6AddrInfo;

	serverAddr = new addrinfo;
	memset(&HINTS, 0, sizeof(HINTS));
	HINTS.ai_family = AF_UNSPEC; //search both IPv4 and IPv6 addrs
	HINTS.ai_socktype = SOCK_DGRAM; //Use UDP
	HINTS.ai_protocol = 17;
	HINTS.ai_flags = AI_PASSIVE; //use wildcard to find local addrs
	int value = getaddrinfo(NULL, port, &HINTS, &AddrInfo);
	if (value != 0) {
		cout << "Didn't resolve to addr info\n";
		return -1;
	}
	v4AddrInfo = NULL;
	v6AddrInfo = NULL;

	while (AddrInfo != NULL) {
		if (AddrInfo->ai_family == AF_INET6) {
			v6AddrInfo = AddrInfo;
		} else if (AddrInfo->ai_family == AF_INET) {
			v4AddrInfo = AddrInfo;
		} else {
			printf("Server port binding returned a non v4 or v6 Addr\n");
		}
		AddrInfo = AddrInfo->ai_next;
	}

	if (v6AddrInfo != NULL) {
		sock = socket(v6AddrInfo->ai_family, v6AddrInfo->ai_socktype,
				v6AddrInfo->ai_protocol);
		if (sock != -1) {
			if (bind(sock, v6AddrInfo->ai_addr, v6AddrInfo->ai_addrlen) != 0) {
				printf("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v6AddrInfo, sizeof(addrinfo));
	} else if (v4AddrInfo != NULL) {
		sock = socket(v4AddrInfo->ai_family, v4AddrInfo->ai_socktype,
				v4AddrInfo->ai_protocol);
		if (sock != -1) {
			if (bind(sock, v4AddrInfo->ai_addr, v4AddrInfo->ai_addrlen) != 0) {
				printf("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			printf("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v4AddrInfo, sizeof(addrinfo));
	} else {
		printf("ERROR: Unable to find any local IP Address\n");
	}
	freeaddrinfo(AddrInfo);

	//TODO: add in 3 way handshake & block until done

	serverSeq = 1; //rand gen
	clientSeq = 1; //sent to us
	recvWindow = MAX_RECV_BUFF;
	window = MAX_RECV_BUFF; //gotten from client sent packets?

	dataList = new OrderedList();
	sem_init(&data_sem, 0, 1);

	if (pthread_create(&recv, NULL, recvServer, (void *) this)) {
		printf("ERROR: unable to create producer thread.\n");
		exit(-1);
	}

	return 0;
}

int TCP::write(char *buffer, unsigned int bufLen) {
	first = 1;
	int index = 0;
	char *packet;
	TCP_hdr *header;
	int packetLen;
	int dataLen;
	unsigned int resendSeq;
	Node *node;
	int ret;
	cout << "entered\n";
	while (1) {
		while ((ret = sem_wait(&state_sem)) == -1 && errno == EINTR)
			;
		if (ret == -1 && errno != EINTR) {
			perror("sem_wait prod");
			exit(-1);
		}

		if (timeoutFlag) {
			timeoutFlag = 0;

			switch (state) {
			case CONTINUE:
				state = GBN;
				first = 1;
				break;
			case GBN:
				first = 1;
				break;
			case FAST:
				state = GBN;
				first = 1;
				break;
			default:
				fprintf(stderr, "unknown state, state=%d", state);
				exit(-1);
			}
		}

		cout << "comparing state\n";
		switch (state) {
		case CONTINUE:
			cout << "in continue\n";
			if (index < bufLen && window > 0) {
				cout << "sending packet\n";
				packet = new char[MTU];
				header = (struct TCP_hdr *) packet;

				header->srcPort = 0; //need to fill in sometime
				header->dstPort = 0; //need to fill in sometime
				header->seqNum = clientSeq;
				header->ack = serverSeq;

				memset(&header->flags, 0, sizeof(short));

				unsigned char offset = 5;
				header->flags |= (offset) << 12; //check this?

				header->window = MAX_RECV_BUFF;
				header->urgent = 0;

				if (bufLen - index > MTU - TCP_HEADER_SIZE) {
					dataLen = MTU - TCP_HEADER_SIZE;
				} else {
					dataLen = bufLen - index;
				}
				if (dataLen > window) { //leave for now, move to outside if for Nagle
					dataLen = window;
				}
				//Window check / congestion window calc to determine dataLen
				//also where Nagle alg is

				memcpy(&header->options, &buffer[index], dataLen); //ok if we have no options, change if we do
				index += dataLen;
				clientSeq += dataLen;
				packetLen = TCP_HEADER_SIZE + dataLen;

				header->checksum = checksum(packet, packetLen);

				while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
					;
				if (ret == -1 && errno != EINTR) {
					perror("sem_wait prod");
					exit(-1);
				}
				cout << "list insertion\n";
				int ret = packetList->insert(header->seqNum, header->seqNum
						+ dataLen - 1, packet, packetLen);
				if (ret) {
					//should never occur
				}
				cout << "actually inserted..\n";
				window -= dataLen;
				send(sock, packet, packetLen, 0);

				sem_post(&packet_sem);

				if (first) {
					first = 0;
					setTimeoutTimer(to_timer, 0);
				}
				cout << "finished packet send\n";
			} else {
				waitFlag = 1;
			}
			break;
		case GBN:
			cout << "WENT TO GO BACK NNNNN!!!!!!\n";
			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}

			if (first) {
				node = packetList->peekHead();
			} else if (window <= 0) {
				node = NULL;
				waitFlag = 1;
			} else {
				node = packetList->findNext(resendSeq);
			}

			if (node != NULL) {
				resendSeq = node->seqNum;
				send(sock, node->data, node->size, 0);
				window -= dataLen;
				if (first) {
					first = 0;
					setTimeoutTimer(to_timer, 0);
				}
			} else {
				state = CONTINUE;
				first = 0;
			}
			sem_post(&packet_sem);
			break;
		case FAST:
			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}

			node = packetList->peekHead();
			if (node != NULL) {
				send(sock, node->data, node->size, 0);
			}

			state = CONTINUE;

			sem_post(&packet_sem);
			break;
		default:
			perror("State error, exiting.");
			exit(1);
		}

		if (timeoutFlag) {
			timeoutFlag = 0;
			waitFlag = 0; //handle cases where TO after set waitFlag
			sem_init(&write_sem, 0, 0);

			switch (state) {
			case CONTINUE:
				state = GBN;
				first = 1;
				break;
			case GBN:
				first = 1;
				break;
			case FAST:
				state = GBN;
				first = 1;
				break;
			default:
				fprintf(stderr, "unknown state, state=%d", state);
				exit(-1);
			}
		}
		sem_post(&state_sem);

		if (waitFlag) {
			cout << "Waiting...\n";
			while ((ret = sem_wait(&write_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}
			waitFlag = 0;
		}
	}
}
