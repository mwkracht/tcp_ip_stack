/*
 * Jonathan Reed - 9051-66446
 * Matthew Kracht - 9053_25165
 * ECE - 5565
 * Project 2
 */

#include "TCP.h"

unsigned int timeoutFlag;

unsigned int firstFlag;
unsigned int gbnFlag;
unsigned int fastFlag;

unsigned int waitFlag;
sem_t wait_sem;

unsigned int TOTAL_timeouts = 0;
unsigned int TOTAL_fast = 0;
unsigned int TOTAL_gbn = 0;
unsigned int TOTAL_dropped = 0;
unsigned int TOTAL_wait = 0;

double TCP::stopTimer(timer_t timer) {
	PRINT_DEBUG("stopping timer\n");
	struct itimerspec its;
	struct itimerspec its_old;

	its.it_value.tv_sec = 0;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;

	if (timer_settime(timer, 0, &its, &its_old) == -1) {
		PRINT_ERROR("Error setting timer.\n");
		exit(-1);
	}

	return its_old.it_value.tv_sec * 1000.0 + its_old.it_value.tv_nsec
			/ 1000000.0;
}

void TCP::startTimer(timer_t timer, double millis) {
	PRINT_DEBUG("starting timer m=%f\n", millis);
	struct itimerspec its;
	its.it_value.tv_sec = static_cast<long int> (millis / 1000);
	its.it_value.tv_nsec = static_cast<long int> (fmod(millis, 1000) * 1000000);
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;

	if (timer_settime(timer, 0, &its, NULL) == -1) {
		PRINT_ERROR("Error setting timer.\n");
		exit(-1);
	}
}

unsigned int TCP::setTimer(timer_t timer, int millis) {
	struct itimerspec its;
	its.it_value.tv_sec = millis / 1000;
	its.it_value.tv_nsec = (millis % 1000) * 1000000;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;

	struct itimerspec its_old;
	if (timer_settime(timer, 0, &its, &its_old) == -1) {
		PRINT_ERROR("Error setting timer.\n");
		exit(-1);
	}

	return its_old.it_value.tv_sec * 1000 + its_old.it_value.tv_nsec / 1000000;
}

static void to_handler(int sig, siginfo_t *si, void *uc) {
	PRINT_DEBUG("timeout acquired and posted\n");
	TOTAL_timeouts++;
	timeoutFlag = 1;
	if (waitFlag == 1) {
		//PRINT_DEBUG("posting to wait_sem\n");
		sem_post(&wait_sem);
	}
}

static void read_handler(int sig, siginfo_t *si, void *uc) {
	//PRINT_DEBUG("read acquired and posted\n");
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
	char *buffer = new char[MTU];
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
	double sampRTT;
	double alpha = 0.125, beta = 0.25;
	struct timeval current;

	peerAddr = new sockaddr_storage;
	peerLen = sizeof(struct sockaddr_storage);

	while (1) {

		size = recvfrom(myTCP->sock, (void *) buffer, MTU, 0,
				(struct sockaddr *) peerAddr, &peerLen);
		if (size == -1) {
			PRINT_ERROR("ERROR receiving data over socket: %s\n", strerror(errno));
			continue;
		}

		header = (struct TCP_hdr *) buffer;
		offset = 4 * (header->flags >> 12);
		data = (char *) header + offset;
		dataLen = size - offset;

		PRINT_DEBUG("Received packet, seq=%d, size=%d\n", header->seqNum, size);

		//check that ports match expected or reject packet
		calc = checksum(buffer, size);
		if (calc != header->checksum) {
			PRINT_ERROR("recvChecksum:%u calcChecksum:%u\n", header->checksum, calc);
		} else {
			if (header->flags & ACK_FLAG) {

				while ((ret = sem_wait(&myTCP->packet_sem)) == -1 && errno
						== EINTR)
					;
				if (ret == -1 && errno != EINTR) {
					PRINT_ERROR("sem_wait prod");
					exit(-1);
				}

				PRINT_DEBUG("before recvWin=%d Win=%d\n", myTCP->recvWindow,
						myTCP->window);

				myTCP->window = header->window;
				myTCP->serverSeq = header->seqNum;

				PRINT_DEBUG("after recvWin=%d Win=%d\n", myTCP->recvWindow,
						myTCP->window);

				PRINT_DEBUG("before cong: state=%d win=%f\n", myTCP->congState,
						myTCP->congWindow);

				head = myTCP->packetList->peekHead();
				Node *tail = myTCP->packetList->peekTail();
				if (head != NULL) {
					if (tail) {
						PRINT_DEBUG("Sendbase=%u end=%u ACK=%u\n", head->seqNum,
								tail->seqEnd, header->ack);
					} else {
						PRINT_DEBUG("Sendbase=%u end=%u ACK=%u\n", head->seqNum,
								myTCP->clientSeq, header->ack);
					}
					if (header->ack == head->seqNum) {
						if (FR_FLAG)
							dupSeq++;
						if (gbnFlag) {
							firstFlag = 1;
						}

						if (dupSeq == 3) {
							dupSeq = 0;

							myTCP->startTimer(myTCP->to_timer, myTCP->timeout);
							//myTCP->validRTT = 0;
							PRINT_DEBUG("dropping seqEndRTT=%d\n", myTCP->seqEndRTT);
							myTCP->seqEndRTT = 0;

							switch (myTCP->congState) {
							case SLOWSTART:
								myTCP->congState = FASTREC;
								myTCP->threshhold = myTCP->congWindow / 2;
								if (myTCP->threshhold < myTCP->MSS) {
									myTCP->threshhold = myTCP->MSS;
								}
								myTCP->congWindow = myTCP->threshhold + 3
										* myTCP->MSS;
								fastFlag = 1;
								break;
							case AIMD:
								myTCP->congState = FASTREC;
								myTCP->threshhold = myTCP->congWindow / 2;
								if (myTCP->threshhold < myTCP->MSS) {
									myTCP->threshhold = myTCP->MSS;
								}
								myTCP->congWindow = myTCP->threshhold + 3
										* myTCP->MSS;
								fastFlag = 1;
								break;
							case FASTREC:
								fastFlag = 0;
								break;
							default:
								PRINT_ERROR("unknown congState=%d\n",
										myTCP->congState)
								;
							} PRINT_DEBUG("fast retransmit\n");
						}
					} else {
						if (myTCP->packetList->containsEnd(header->ack - 1)) {
							dupSeq = 0;

							//sampRTT = myTCP->timeout - myTCP->stopTimer(
							//		myTCP->to_timer);

							if (gbnFlag) {
								firstFlag = 1;
							}

							if (RTT_FLAG && myTCP->seqEndRTT == header->ack) {

								gettimeofday(&current, 0);

								PRINT_DEBUG("getting seqEndRTT=%d stampRTT=(%d, %d)\n", myTCP->seqEndRTT, myTCP->stampRTT.tv_sec, myTCP->stampRTT.tv_usec); PRINT_DEBUG("getting seqEndRTT=%d current=(%d, %d)\n", myTCP->seqEndRTT, current.tv_sec, current.tv_usec);

								PRINT_DEBUG("old sampleRTT=%f estRTT=%f devRTT=%f timout=%f\n", sampRTT, myTCP->estRTT, myTCP->devRTT, myTCP->timeout);

								myTCP->seqEndRTT = 0;

								if (myTCP->stampRTT.tv_usec > current.tv_usec) {
									double decimal = (1000000.0
											+ current.tv_usec
											- myTCP->stampRTT.tv_usec)
											/ 1000000.0;
									sampRTT = current.tv_sec
											- myTCP->stampRTT.tv_sec - 1.0;
									sampRTT += decimal;
								} else {
									double decimal = (current.tv_usec
											- myTCP->stampRTT.tv_usec)
											/ 1000000.0;
									sampRTT = current.tv_sec
											- myTCP->stampRTT.tv_sec;
									sampRTT += decimal;
								}
								sampRTT *= 1000.0;

								if (myTCP->firstRTT) {
									myTCP->estRTT = sampRTT;
									myTCP->devRTT = sampRTT / 2;
									myTCP->firstRTT = 0;
								}

								myTCP->estRTT = (1 - alpha) * myTCP->estRTT
										+ alpha * sampRTT;
								myTCP->devRTT = (1 - beta) * myTCP->devRTT
										+ beta * fabs(sampRTT - myTCP->estRTT);

								myTCP->timeout = myTCP->estRTT + myTCP->devRTT
										/ beta;
								if (myTCP->timeout < MIN_TIMEOUT) {
									myTCP->timeout = MIN_TIMEOUT;
								} else if (myTCP->timeout > MAX_TIMEOUT) {
									myTCP->timeout = MAX_TIMEOUT;
								}

								PRINT_DEBUG("new sampleRTT=%f estRTT=%f devRTT=%f timout=%f\n", sampRTT, myTCP->estRTT, myTCP->devRTT, myTCP->timeout);
								//PRINT_DEBUG("sampleRTT=%f timout=%f\n", sampRTT, myTCP->timeout);
							}

							myTCP->startTimer(myTCP->to_timer, myTCP->timeout);
							//PRINT_DEBUG("RTT=%u\n", RTT);
							switch (myTCP->congState) {
							case SLOWSTART:
								myTCP->congWindow += myTCP->MSS;
								if (myTCP->congWindow >= myTCP->threshhold) {
									myTCP->congState = AIMD;
								}
								break;
							case AIMD:
								myTCP->congWindow += myTCP->MSS * myTCP->MSS
										/ myTCP->congWindow;
								break;
							case FASTREC:
								myTCP->congState = AIMD;
								myTCP->congWindow = myTCP->threshhold;
								break;
							default:
								PRINT_ERROR("unknown congState=%d\n",
										myTCP->congState)
								;
							} PRINT_DEBUG("congState=%d congWindow=%f\n",
									myTCP->congState, myTCP->congWindow);

							while (head != NULL && header->ack > head->seqEnd) {
								myTCP->packetList->removeHead();
								head = myTCP->packetList->peekHead();
							}
						} else {
							PRINT_ERROR("invalid ack\n");
						}
					}
					//PRINT_DEBUG("Through processing ack\n");

				} PRINT_DEBUG("after cong: state=%d win=%f\n", myTCP->congState,
						myTCP->congWindow);

				sem_post(&myTCP->packet_sem);

				if (waitFlag) {
					PRINT_DEBUG("posting to wait_sem\n");
					sem_post(&wait_sem);
				}

			} else {
				PRINT_ERROR("Error: Client received packet without ACK set.");
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
	char ackBuf[myTCP->MSS + TCP_HEADER_SIZE];
	socklen_t peerLen;
	unsigned short calc;
	struct sockaddr_storage *peerAddr;
	int ret;

	myTCP->packetList = new OrderedList();
	peerAddr = new sockaddr_storage;
	peerLen = sizeof(struct sockaddr_storage);

	while (1) {
		if (newBuf) {
			buffer = new char[myTCP->MSS + TCP_HEADER_SIZE];
		}

		size = recvfrom(myTCP->sock, (void *) buffer, myTCP->MSS
				+ TCP_HEADER_SIZE, 0, (struct sockaddr *) peerAddr, &peerLen);
		if (size == -1) {
			PRINT_ERROR("ERROR receiving data over socket: %s\n", strerror(errno));
			continue;
		}

		header = (struct TCP_hdr *) buffer;
		offset = 4 * (header->flags >> 12);
		data = (char *) header + offset;
		dataLen = size - offset;

		PRINT_DEBUG("Received packet, seq=%d, size=%d\n", header->seqNum, size);

		//check that ports match expected or reject packet
		calc = checksum(buffer, size);
		if (calc != header->checksum) {
			newBuf = false;
			//NACK send base
			PRINT_ERROR("recvChecksum:%u calcChecksum:%u\n", header->checksum, calc);
		} else {
			while ((ret = sem_wait(&myTCP->data_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				PRINT_ERROR("sem_wait prod");
				exit(-1);
			}
			if (myTCP->clientSeq == header->seqNum) {
				PRINT_DEBUG("In order seq=%d end=%d client=%d\n", header->seqNum,
						header->seqNum + dataLen, myTCP->clientSeq + dataLen);
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
						myTCP->packetList->removeHead();
						header = (struct TCP_hdr *) head->data;

						//Process Flags

						offset = 4 * (header->flags >> 12);
						data = (char *) header + offset;
						dataLen = head->size - offset;
						PRINT_DEBUG("Connected to seq=%d datalen:%d\n",
								header->seqNum, dataLen);

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
				Node *dataHead = myTCP->dataList->peekHead();
				Node *dataTail = myTCP->dataList->peekTail();
				Node *tail = myTCP->packetList->peekTail();
				if (tail) {
					PRINT_DEBUG("clientSeq=%d seqend=%d buffered=%d\n",
							myTCP->clientSeq, tail->seqEnd, tail->seqEnd
							- myTCP->clientSeq);
				}

				//PRINT_DEBUG("Test window:%u\n", myTCP->window);

				//TODO: handle circular seq num
				if (BUF_FLAG && myTCP->clientSeq < header->seqNum
						&& header->seqNum + dataLen <= myTCP->clientSeq
								+ MAX_RECV_BUFF) {
					PRINT_DEBUG("Buffering out of order exp=%d seq=%d sanity=%d\n",
							myTCP->clientSeq, header->seqNum, dataLen);
					int ret = myTCP->packetList->insert(header->seqNum,
							header->seqNum + dataLen - 1, buffer, size);
					if (ret) {
						//is dubplicate / colliding
						newBuf = false;
						PRINT_DEBUG("Dropping duplicate exp=%d seq=%d\n",
								myTCP->clientSeq, header->seqNum);
						TOTAL_dropped++;
					} else {
						myTCP->window -= dataLen;
						newBuf = true;
					}
				} else {
					PRINT_DEBUG("Dropping out of window exp=%d seq=%d\n",
							myTCP->clientSeq, header->seqNum);
					TOTAL_dropped++;
				}
			}

			//ACK
			PRINT_DEBUG("Sending ACK seq=%d\n", myTCP->clientSeq);

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

			PRINT_DEBUG("Ack hdr window:%u\n", myTCP->window);
			//unsigned char *options;
			//data? if we have 2way msging

			ackHdr->checksum = checksum(ackBuf, ackLen);
			sendto(myTCP->sock, ackBuf, ackLen, 0,
					(struct sockaddr *) peerAddr, peerLen);
			myTCP->serverSeq++;

			sem_post(&myTCP->data_sem);
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
		PRINT_ERROR("Didn't resolve to addr info\n");
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
			PRINT_ERROR("Server port binding returned a non v4 or v6 Addr\n");
		}
		AddrInfo = AddrInfo->ai_next;
	}
	if (v6AddrInfo != NULL) {
		sock = socket(v6AddrInfo->ai_family, v6AddrInfo->ai_socktype,
				v6AddrInfo->ai_protocol);
		if (sock != -1) {
			if (connect(sock, v6AddrInfo->ai_addr, v6AddrInfo->ai_addrlen) != 0) {
				PRINT_ERROR("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			PRINT_ERROR("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v6AddrInfo, sizeof(addrinfo));
	} else if (v4AddrInfo != NULL) {
		sock = socket(v4AddrInfo->ai_family, v4AddrInfo->ai_socktype,
				v4AddrInfo->ai_protocol);
		if (sock != -1) {
			if (connect(sock, v4AddrInfo->ai_addr, v4AddrInfo->ai_addrlen) != 0) {
				PRINT_ERROR("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			PRINT_ERROR("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v4AddrInfo, sizeof(addrinfo));
	} else {
		PRINT_ERROR("ERROR: Unable to find any local IP Address\n");
	}
	freeaddrinfo(AddrInfo);

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = to_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
		PRINT_ERROR("Error creating time out timer. Exiting.\n");
		exit(-1);
	}

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_value.sival_ptr = &to_timer;
	if (timer_create(CLOCK_REALTIME, &sev, &to_timer) == -1) {
		PRINT_ERROR("Error creating time out timer. Exiting.\n");
		exit(-1);
	}
	stopTimer(to_timer);

	//TODO: add in 3 way handshake & block until done

	//all these are agreed in setup
	MSS = MTU - TCP_HEADER_SIZE;
	clientSeq = 1; //rand gen
	serverSeq = 1; //sent to us
	recvWindow = MAX_RECV_BUFF;
	window = recvWindow; //sent to us

	congState = SLOWSTART;
	congWindow = MSS;
	//PRINT_DEBUG("congState=%d congWindow=%d\n", congState, congWindow);
	threshhold = recvWindow;
	timeout = 50;
	estRTT = timeout;
	devRTT = 0;
	firstRTT = 1;
	seqEndRTT = 0;

	packetList = new OrderedList();
	sem_init(&packet_sem, 0, 1);
	sem_init(&wait_sem, 0, 0);

	gbnFlag = 0;
	fastFlag = 0;
	timeoutFlag = 0;
	waitFlag = 0;

	if (pthread_create(&recv, NULL, recvClient, (void *) this)) {
		PRINT_ERROR("ERROR: unable to create producer thread.\n");
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
		PRINT_ERROR("Didn't resolve to addr info\n");
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
			PRINT_ERROR("Server port binding returned a non v4 or v6 Addr\n");
		}
		AddrInfo = AddrInfo->ai_next;
	}

	if (v6AddrInfo != NULL) {
		sock = socket(v6AddrInfo->ai_family, v6AddrInfo->ai_socktype,
				v6AddrInfo->ai_protocol);
		if (sock != -1) {
			if (bind(sock, v6AddrInfo->ai_addr, v6AddrInfo->ai_addrlen) != 0) {
				PRINT_ERROR("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			PRINT_ERROR("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v6AddrInfo, sizeof(addrinfo));
	} else if (v4AddrInfo != NULL) {
		sock = socket(v4AddrInfo->ai_family, v4AddrInfo->ai_socktype,
				v4AddrInfo->ai_protocol);
		if (sock != -1) {
			if (bind(sock, v4AddrInfo->ai_addr, v4AddrInfo->ai_addrlen) != 0) {
				PRINT_ERROR("Error in binding socket: %s\n", strerror(errno));
				close(sock);
				freeaddrinfo(AddrInfo);
				return -1;
			}
		} else {
			PRINT_ERROR("Error creating socket: %s\n", strerror(errno));
		}
		memcpy(serverAddr, v4AddrInfo, sizeof(addrinfo));
	} else {
		PRINT_ERROR("ERROR: Unable to find any local IP Address\n");
	}
	freeaddrinfo(AddrInfo);

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = read_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
		PRINT_ERROR("Error creating time out timer. Exiting.\n");
		exit(-1);
	}

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_value.sival_ptr = &to_timer;
	if (timer_create(CLOCK_REALTIME, &sev, &to_timer) == -1) {
		PRINT_ERROR("Error creating time out timer. Exiting.\n");
		exit(-1);
	}
	stopTimer(to_timer);

	//TODO: add in 3 way handshake & block until done

	serverSeq = 1; //rand gen
	clientSeq = 1; //sent to us
	recvWindow = MAX_RECV_BUFF;
	window = MAX_RECV_BUFF; //gotten from client sent packets?
	MSS = MTU - TCP_HEADER_SIZE;

	dataList = new OrderedList();
	sem_init(&data_sem, 0, 1);
	sem_init(&wait_sem, 0, 0);

	if (pthread_create(&recv, NULL, recvServer, (void *) this)) {
		PRINT_ERROR("ERROR: unable to create producer thread.\n");
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
	unsigned char offset;
	int onWire = 0;
	Node *head;

	//PRINT_DEBUG("entered\n");
	while (1) {
		PRINT_DEBUG("Win=%u recvWin=%u congWin=%f\n", window, recvWindow, congWindow); PRINT_DEBUG("flags: first=%d gbn=%d, fast=%d, to=%d, wait=%d\n", firstFlag,
				gbnFlag, fastFlag, timeoutFlag, waitFlag);

		if (timeoutFlag) {
			PRINT_DEBUG("timeout=%f\n", timeout);

			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				PRINT_ERROR("sem_wait prod");
				exit(-1);
			}
			timeoutFlag = 0;
			if (RTT_FLAG)
				timeout *= 2;
			if (timeout > MAX_TIMEOUT) {
				timeout = MAX_TIMEOUT;
			}

			firstFlag = 1;
			gbnFlag = 1;
			fastFlag = 0;

			waitFlag = 0; //handle cases where TO after set waitFlag
			sem_init(&wait_sem, 0, 0);

			switch (congState) {
			case SLOWSTART:
				threshhold = congWindow / 2;
				if (threshhold < MSS) {
					threshhold = MSS;
				}
				congState = AIMD;
				congWindow = threshhold;
				break;
			case AIMD:
				threshhold = recvWindow;
				congState = SLOWSTART;
				congWindow = MSS;
				break;
			case FASTREC:
				threshhold = recvWindow;
				congState = SLOWSTART;
				congWindow = MSS;
				break;
			default:
				PRINT_ERROR("unknown congState=%d\n",
						congState)
				;
			}

			sem_post(&packet_sem);

			PRINT_DEBUG("congState=%d congWindow=%f\n", congState, congWindow);
		} else if (fastFlag) {
			PRINT_DEBUG("fast retransmit\n");

			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				PRINT_ERROR("sem_wait prod");
				exit(-1);
			}
			fastFlag = 0;

			node = packetList->peekHead();
			if (node != NULL) {
				send(sock, node->data, node->size, 0);
				PRINT_DEBUG("fast: sending seqnum=%d seqend=%d size=%d\n",
						node->seqNum, node->seqEnd, node->size);
				TOTAL_fast++;
			}
			sem_post(&packet_sem);
		} else if (gbnFlag) {
			PRINT_DEBUG("WENT TO GO BACK NNNNN!!!!!!\n");
			while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				PRINT_ERROR("sem_wait prod");
				exit(-1);
			}

			PRINT_DEBUG("packetlist.size=%d\n", packetList->getSize());

			head = packetList->peekHead();
			if (head) {
				onWire = clientSeq - head->seqNum;
			} else {
				onWire = 0;
			}
			cong = congWindow - onWire;

			if (head) {
				PRINT_DEBUG("send_base=%d onWire=%d cong=%d\n", head->seqNum,
						onWire, cong);
			} else {
				PRINT_DEBUG("send_base=%d onWire=0 cong=%d\n", clientSeq, cong);
			}

			if (firstFlag) {
				node = head;
			} else if ((window <= 0 || cong <= 0) && CW_FLAG) { //congestion window
				node = NULL;
				waitFlag = 1;
				PRINT_DEBUG("flagging waitFlag\n");
			} else {
				node = packetList->findNext(resendSeq);
			}

			if (node != NULL) {
				resendSeq = node->seqNum;

				send(sock, node->data, node->size, 0);

				header = (struct TCP_hdr *) node->data;
				offset = 4 * (header->flags >> 12);
				dataLen = node->size - offset;
				window -= dataLen;

				PRINT_DEBUG("gbn: sending seqnum=%d seqend=%d size=%d dataLen=%d\n",
						node->seqNum, node->seqEnd, node->size, dataLen);
				TOTAL_gbn++;

				if (firstFlag) {
					firstFlag = 0;

					startTimer(to_timer, timeout);
					PRINT_DEBUG("dropping seqEndRTT=%d\n", seqEndRTT);
					seqEndRTT = 0;
				}
			} else if (!waitFlag) {
				firstFlag = 0;
				gbnFlag = 0;
				PRINT_DEBUG("stopping gbn\n");
			}
			sem_post(&packet_sem);
		} else {
			PRINT_DEBUG("congWin:%u wind:%u\n", static_cast<unsigned int> (congWindow),
					window);
			head = packetList->peekHead();
			if (head) {
				onWire = clientSeq - head->seqNum;
			} else {
				onWire = 0;
			}
			cong = congWindow - onWire;

			if (head) {
				PRINT_DEBUG("send_base=%d onWire=%d cong=%d\n", head->seqNum,
						onWire, cong);
			} else {
				PRINT_DEBUG("send_base=%d onWire=0 cong=%d\n", clientSeq, cong);
			}
			if (index < bufLen && (window > 0 && cong > 0 && cong >= MSS
					|| !CW_FLAG) && onWire < recvWindow) {
				PRINT_DEBUG("sending packet\n");
				packet = new char[MTU];
				header = (struct TCP_hdr *) packet;

				header->srcPort = 0; //need to fill in sometime
				header->dstPort = 0; //need to fill in sometime
				header->seqNum = clientSeq;
				header->ack = serverSeq;

				memset(&header->flags, 0, sizeof(short));

				offset = 5;
				header->flags |= (offset) << 12; //check this?

				header->window = MAX_RECV_BUFF;
				header->urgent = 0;

				if (bufLen - index > MSS) {
					dataLen = MSS;
				} else {
					dataLen = bufLen - index;
				}
				if (dataLen > window && CW_FLAG) { //leave for now, move to outside if for Nagle

					dataLen = window;
				}
				if (dataLen > cong && CW_FLAG) {
					dataLen = cong;
				}
				//				PRINT_DEBUG("dataLen:%u\n",dataLen);
				//Window check / congestion window calc to determine dataLen
				//also where Nagle alg is

				memcpy(&header->options, &buffer[index], dataLen); //ok if we have no options, change if we do
				index += dataLen;
				clientSeq += dataLen;
				packetLen = TCP_HEADER_SIZE + dataLen;

				header->checksum = checksum(packet, packetLen);

				PRINT_DEBUG("hung up on packet sem\n");
				while ((ret = sem_wait(&packet_sem)) == -1 && errno == EINTR)
					;
				if (ret == -1 && errno != EINTR) {
					PRINT_ERROR("sem_wait prod");
					exit(-1);
				} PRINT_DEBUG("broke packet sem\n");
				int ret = packetList->insert(header->seqNum, header->seqNum
						+ dataLen - 1, packet, packetLen);
				if (ret) {
					//should never occur
				}
				//				PRINT_DEBUG("actually inserted..\n");
				window -= dataLen;
				send(sock, packet, packetLen, 0);
				PRINT_DEBUG(
						"cont: sending seqnum=%d seqend=%d size=%d dataLen=%d\n",
						header->seqNum, header->seqNum + dataLen - 1, dataLen
						+ 20, dataLen);

				if (seqEndRTT == 0) {
					//validRTT = 1;
					gettimeofday(&stampRTT, 0);
					seqEndRTT = clientSeq;
					PRINT_DEBUG("setting seqEndRTT=%d stampRTT=(%d, %d)\n", seqEndRTT, stampRTT.tv_sec, stampRTT.tv_usec);
				}

				if (firstFlag) {
					firstFlag = 0;
					startTimer(to_timer, timeout);
				}

				sem_post(&packet_sem);

				PRINT_DEBUG("finished packet send\n");
			} else {
				waitFlag = 1;
				PRINT_DEBUG("flagging waitFlag\n"); PRINT_DEBUG("cases: index=%d window=%d cong=%d MSS=%d recvWin=%d\n", index < bufLen, window > 0, cong > 0, cong >= MSS, onWire < recvWindow);
			}
		}

		if (index >= bufLen && packetList->getSize() == 0) {
			PRINT_DEBUG("finished write bufLen=%d, returning\n", bufLen); PRINT_DEBUG("totals: to=%d fast=%d wait=%d\n", TOTAL_timeouts,
					TOTAL_fast, TOTAL_wait);
			waitFlag = 0;
			stopTimer(to_timer);
			return 0;
		}

		if (waitFlag && !timeoutFlag && !fastFlag) {
			PRINT_DEBUG("Waiting...\n");
			TOTAL_wait++;

			while ((ret = sem_wait(&wait_sem)) == -1 && errno == EINTR)
				;
			if (ret == -1 && errno != EINTR) {
				PRINT_ERROR("sem_wait prod");
				exit(-1);
			}
			waitFlag = 0;
			sem_init(&wait_sem, 0, 0);

			PRINT_DEBUG("left waiting waitFlag=%d\n", waitFlag);
		}
	}
}

int TCP::read(char *buffer, unsigned int bufLen, double millis) {
	int size = 0;
	char *index = buffer;
	char *pt;
	int ret = 0;
	Node *head = NULL;
	Node *tail;
	int avail = 0;
	timeoutFlag = 0;

	if (millis != 0.0)
		startTimer(to_timer, millis);

	while (!timeoutFlag && size < bufLen) {
		PRINT_DEBUG("Waiting size=%d bufLen=%d\n", size, bufLen);

		while ((ret = sem_wait(&wait_sem)) == -1 && errno == EINTR)
			;
		if (ret == -1 && errno != EINTR) {
			PRINT_ERROR("sem_wait prod");
			exit(-1);
		}

		//PRINT_DEBUG("Waiting data_sem\n");
		while ((ret = sem_wait(&data_sem)) == -1 && errno == EINTR)
			;
		if (ret == -1 && errno != EINTR) {
			PRINT_ERROR("sem_wait prod");
			exit(-1);
		}
		sem_init(&wait_sem, 0, 0);

		PRINT_DEBUG("dataList.size=%d\n", dataList->getSize());
		Node *node = dataList->peekHead();
		if (node) {
			//PRINT_DEBUG(" seqnum=%d", node->seqNum);
		}
		node = dataList->peekTail();
		if (node) {
			//PRINT_DEBUG(" seqend=%d", node->seqEnd);
		}
		//PRINT_DEBUG("\n");

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
				PRINT_DEBUG("window=%d\n", window);
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

	PRINT_DEBUG("totals: dropped=%d\n", TOTAL_dropped);

	stopTimer(to_timer);

	return size;
}
