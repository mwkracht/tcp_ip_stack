#include "TCP.h"

unsigned int firstFlag;
unsigned int gbnFlag;
unsigned int fastFlag;
unsigned int timeoutFlag;
unsigned int waitFlag;
sem_t wait_sem;

void TCP::setTimeoutTimer(timer_t timer, int millis) {
	struct itimerspec its;
	its.it_value.tv_sec = millis / 1000;
	its.it_value.tv_nsec = (millis % 1000) * 1000000;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;

	if (timer_settime(timer, 0, &its, NULL) == -1) {
		printf("Error setting timer.\n");
		exit(-1);
	}
}

static void to_handler(int sig, siginfo_t *si, void *uc) {
	cout << "timeout acquired and posted\n";
	timeoutFlag = 1;
	if (waitFlag == 1) {
		cout << "posting to wait_sem\n";
		sem_post(&wait_sem);
	}
}

static void read_handler(int sig, siginfo_t *si, void *uc) {
	cout << "read acquired and posted\n";
	timeoutFlag = 1;
	sem_post(&wait_sem);
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
	char *buffer = new char[myTCP->MSS];
	struct TCP_hdr *header;
	unsigned short offset;
	unsigned int dataLen;
	unsigned short calc;
	int dupSeq = 0;
	int ret;
	char *data;
	int size;
	Node *head;
	socklen_t peerLen;
	struct sockaddr_storage *peerAddr;

	peerAddr = new sockaddr_storage;
	peerLen = sizeof(struct sockaddr_storage);

	while (1) {

		size = recvfrom(myTCP->sock, (void *) buffer, myTCP->MSS, 0,
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

				while ((ret = sem_wait(&myTCP->packet_sem)) == -1 && errno
						== EINTR)
					;
				if (ret == -1 && errno != EINTR) {
					perror("sem_wait prod");
					exit(-1);
				}

				myTCP->recvWindow = header->window;
				myTCP->window = header->window;
				myTCP->serverSeq = header->seqNum;

				printf("recvWin=%d Win=%d\n", myTCP->recvWindow, myTCP->window);

				head = myTCP->packetList->peekHead();
				if (head != NULL) {
					printf("Sendbase=%u ACK=%u\n", head->seqNum, header->ack);
					if (header->ack == head->seqNum) {
						dupSeq++;
						if (dupSeq == 3) {
							dupSeq = 0;

							myTCP->setTimeoutTimer(myTCP->to_timer, 50);
							switch (myTCP->congState) {
							case SLOWSTART:
								myTCP->congState = FASTREC;
								break;
							case AIMD:
								myTCP->congState = FASTREC;
								myTCP->congWindow /= 2;
								break;
							case FASTREC:
								break;
							default:
								fprintf(stderr, "unknown congState=%d\n",
										myTCP->congState);
							}
							printf("congState=%d congWindow=%d\n",
									myTCP->congState, myTCP->congWindow);
							fastFlag = 1;
						}
					} else {
						if (myTCP->packetList->containsEnd(header->ack - 1)) {
							dupSeq = 0;

							myTCP->setTimeoutTimer(myTCP->to_timer, 50);
							switch (myTCP->congState) {
							case SLOWSTART:
								myTCP->congWindow *= 2;
								if (myTCP->congWindow >= myTCP->threshhold) {
									myTCP->congState = AIMD;
								}
								break;
							case AIMD:
								myTCP->congWindow += myTCP->MSS;
								break;
							case FASTREC:
								myTCP->congState = AIMD;
								break;
							default:
								fprintf(stderr, "unknown congState=%d\n",
										myTCP->congState);
							}
							printf("congState=%d congWindow=%d\n",
									myTCP->congState, myTCP->congWindow);

							while (head != NULL && header->ack > head->seqEnd) {
								myTCP->packetList->removeHead();
								head = myTCP->packetList->peekHead();
							}
						}
					}
					printf("Through processing ack\n");
					sem_post(&myTCP->packet_sem);
				}

				if (waitFlag) {
					printf("posting to wait_sem\n");
					sem_post(&wait_sem);
				}

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
	char ackBuf[myTCP->MSS];
	socklen_t peerLen;
	unsigned short calc;
	struct sockaddr_storage *peerAddr;
	int ret;

	myTCP->packetList = new OrderedList();
	peerAddr = new sockaddr_storage;
	peerLen = sizeof(struct sockaddr_storage);

	while (1) {
		if (newBuf) {
			buffer = new char[myTCP->MSS];
		}

		size = recvfrom(myTCP->sock, (void *) buffer, myTCP->MSS, 0,
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
						+ dataLen - 1, data, dataLen, buffer);
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
								+ dataLen - 1, data, dataLen, head->del);
						myTCP->clientSeq += dataLen;
					} else {
						break;
					}
				}

				sem_post(&wait_sem); //signal recv main thread
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
			//ackHdr->ack = myTCP->clientSeq;
			ackHdr->ack = 1;
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
			myTCP->serverSeq++;
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

	MSS = MTU;
	clientSeq = 1; //rand gen
	serverSeq = 1; //sent to us

	recvWindow = MAX_RECV_BUFF;
	window = recvWindow; //sent to us
	congState = SLOWSTART;
	congWindow = MSS - TCP_HEADER_SIZE;
	printf("congState=%d congWindow=%d\n", congState, congWindow);
	threshhold = MAX_RECV_BUFF / 2;

	packetList = new OrderedList();
	sem_init(&packet_sem, 0, 1);
	sem_init(&wait_sem, 0, 0);

	waitFlag = 0;
	timeoutFlag = 0;
	fastFlag = 0;
	gbnFlag = 0;

	if (pthread_create(&recv, NULL, recvClient, (void *) this)) {
		printf("ERROR: unable to create producer thread.\n");
		exit(-1);
	}

	return 0;
}

int TCP::listenTCP(char *port) {

	struct sigevent sev;
	struct sigaction sa;
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

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = read_handler;
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

	serverSeq = 1; //rand gen
	clientSeq = 1; //sent to us
	recvWindow = MAX_RECV_BUFF;
	window = MAX_RECV_BUFF; //gotten from client sent packets?
	MSS = MTU;

	dataList = new OrderedList();
	sem_init(&data_sem, 0, 1);
	sem_init(&wait_sem, 0, 0);

	if (pthread_create(&recv, NULL, recvServer, (void *) this)) {
		printf("ERROR: unable to create producer thread.\n");
		exit(-1);
	}

	return 0;
}

int TCP::write(char *buffer, unsigned int bufLen) {
	firstFlag = 1;
	int index = 0;
	char *packet;
	TCP_hdr *header;
	int packetLen;
	int dataLen;
	unsigned int resendSeq;
	Node *node;
	int cong;
	int ret;
	cout << "entered\n";
	while (1) {
		printf("Win=%d recvWin=%d congWin=%d\n", window, recvWindow, congWindow);
		printf("flags: first=%d gbn=%d, fast=%d, to=%d, wait=%d\n", firstFlag,
				gbnFlag, fastFlag, timeoutFlag, waitFlag);

		if (timeoutFlag) {
			firstFlag = 1;
			gbnFlag = 1;
			fastFlag = 0;
			timeoutFlag = 0;
			waitFlag = 0; //handle cases where TO after set waitFlag
			sem_init(&wait_sem, 0, 0);

			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}
			congState = SLOWSTART;
			congWindow = 1;
			sem_post(&packet_sem);

			printf("congState=%d congWindow=%d\n", congState, congWindow);
		} else if (fastFlag) {
			cout << "fast retransmit\n";
			fastFlag = 0;

			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}
			node = packetList->peekHead();
			if (node != NULL) {
				send(sock, node->data, node->size, 0);
				printf("sending seqnum=%d seqend=%d\n", node->seqNum,
						node->seqEnd);
			}
			sem_post(&packet_sem);
		} else if (gbnFlag) {
			cout << "WENT TO GO BACK NNNNN!!!!!!\n";
			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}

			if (firstFlag) {
				node = packetList->peekHead();
			} else if (window <= 0 || congWindow - recvWindow + window <= 0) { //congestion window
				node = NULL;
				waitFlag = 1;
			} else {
				node = packetList->findNext(resendSeq);
			}

			if (node != NULL) {
				resendSeq = node->seqNum;
				send(sock, node->data, node->size, 0);
				printf("sending seqnum=%d seqend=%d\n", node->seqNum,
						node->seqEnd);
				//window -= dataLen; fix??
				if (firstFlag) {
					firstFlag = 0;
					setTimeoutTimer(to_timer, 50);
				}
			} else {
				gbnFlag = 0;
				firstFlag = 0;
			}
			sem_post(&packet_sem);
		} else {
			cout << "in continue\n";
			cong = congWindow - recvWindow + window;
			if (index < bufLen && window > 0 && cong > 0) {
				cout << "sending packet\n";
				packet = new char[MSS];
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

				if (bufLen - index > MSS - TCP_HEADER_SIZE) {
					dataLen = MSS - TCP_HEADER_SIZE;
				} else {
					dataLen = bufLen - index;
				}
				if (dataLen > window) { //leave for now, move to outside if for Nagle
					dataLen = window;
				}
				if (dataLen > cong) {
					dataLen = cong;
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
				int ret = packetList->insert(header->seqNum, header->seqNum
						+ dataLen - 1, packet, packetLen);
				if (ret) {
					//should never occur
				}
				cout << "actually inserted..\n";
				window -= dataLen;
				send(sock, packet, packetLen, 0);
				printf("sending seqnum=%d seqend=%d\n", header->seqNum,
						header->seqNum + dataLen - 1);
				sem_post(&packet_sem);

				if (firstFlag) {
					firstFlag = 0;
					setTimeoutTimer(to_timer, 50);
				}
				cout << "finished packet send\n";
			} else {
				waitFlag = 1;
			}
		}

		if (index >= bufLen && packetList->getSize() == 0) {
			printf("finished write bufLen=%d, returning\n", bufLen);
			setTimeoutTimer(to_timer, 0);
			return 0;
		}

		if (waitFlag && !timeoutFlag) {
			cout << "Waiting...\n";
			while ((ret = sem_wait(&wait_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				perror("sem_wait prod");
				exit(-1);
			}
			sem_init(&wait_sem, 0, 0);

			waitFlag = 0;
			printf("left waiting waitFlag=%d\n", waitFlag);
		}
	}
}

int TCP::read(char *buffer, unsigned int bufLen, int millis) {
	int size = 0;
	char *index = buffer;
	char *pt;
	int ret = 0;
	Node *head = NULL;
	Node *tail;
	int avail = 0;
	timeoutFlag = 0;

	setTimeoutTimer(to_timer, millis);

	while (!timeoutFlag && size < bufLen) {
		printf("Waiting size=%d bufLen=%d\n", size, bufLen);

		while ((ret = sem_wait(&wait_sem)) == -1 && errno == EINTR)
			;
		if (ret == -1 && errno != EINTR) {
			perror("sem_wait prod");
			exit(-1);
		}
		sem_init(&wait_sem, 0, 0);

		printf("Waiting data_sem\n");
		while ((ret = sem_wait(&data_sem)) == -1 && errno == EINTR)
			;
		if (ret == -1 && errno != EINTR) {
			perror("sem_wait prod");
			exit(-1);
		}

		printf("dataList.size=%d", dataList->getSize());
		Node *node = dataList->peekHead();
		if (node) {
			printf(" seqnum=%d", node->seqNum);
		}
		node = dataList->peekTail();
		if (node) {
			printf(" seqend=%d", node->seqEnd);
		}
		printf("\n");

		while (dataList->getSize() && size < bufLen) {
			head = dataList->peekHead();
			avail = head->size - head->index;
			if (avail) {
				pt = head->data + head->index;
				if (size + avail < bufLen) {
					memcpy(index, pt, avail);

					dataList->removeHead();
					delete head->del;
					delete head;
				} else {
					avail = bufLen - size;
					memcpy(index, pt, avail);

					head->index += avail;
				}

				index += avail;
				size += avail;
				window += avail;
				printf("window=%d\n", window);
			} else {
				dataList->removeHead();
				delete head->del;
				delete head;
			}
		}

		sem_post(&data_sem);

		if (millis == 0) {
			break;
		}
	}

	setTimeoutTimer(to_timer, 0);

	return size;
}
