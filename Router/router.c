/**
 * Router program
 * This program is a plain UDP port forwarder with the added twist that it 
 * uses Srinidhi Varadarajan's libnet's u_sendto() in the forwarding direction to introduce 
 * packet loss delay, and bottleneck bandwidth.
 *
 * Godmar Back
 * January 2005
 *
 * Updated to support IPv6 February 2012
 * <gback@cs.vt.edu>
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include "libnet.h"

static void
usage(char *msg, char *prg)
{
	fprintf(stderr, "%s\n"
		"Usage: %s\n"
		" -C <localport:remotehost:remoteport>\n"
		"          localport is the port on which router is receiving\n"
	        "                    packets from Link 1\n"
		"          remotehost/port is the host/port to which Link 2 connects\n"
		" -B <bandwidth>    link bandwidth in bps\n"
		" -D <delay>        link delay in microseconds\n"
		" -L <tx_loss_prob> transmit loss probability, in fractions of 1000\n"
		" -b  introduce bandwidth variation\n"
		" -t  introduce link delay variation\n"
		" -u  use plain udp forwarding, this overrides -L, -b, and -t flags\n"
		" -v  vary random sequence for bandwidth/rtt variations\n"
		, msg, prg);
	fprintf(stderr, 
		"\nThe DROPS field shown in the statistics has the format m(+n)\n"
		"m is the number of packets intentionally lost along Link 2 \n"
		"  or dropped for bad length\n"
		"n is the number of UDP packets dropped by this machine since the\n"
		"  router program started, as shown in /proc/net/snmp Udp: InErrors.\n"
		"  The latter is a host-wide indicator, so it may not show loss due\n"
	        "  to this instance of router if other students are running another\n"
		"  router instance on the same machine.\n");
	exit(-1);
}

#define MAXPACKET 8192

/* we see ourselves as between a client and a server */
static int clientsocket;		// socket to client
static int serversocket;		// socket to server
// byte counters
static long totalfromclient;
static long totaltoclient;
static long totalfromserver;
static long totaltoserver;
static char *serverhost;		// name of server's host
static char *serverport;		// server port
static struct sockaddr_storage serveraddr;	// server's addr - specified on command line
static int serveraddrlen;
static struct sockaddr_storage clientaddr;	// learned after first recv
static socklen_t clientaddrlen;

static unsigned int get_udp_errors();
static unsigned int initialudperrors;

static void
update()
{
	static int inupdate = 0;
	if (inupdate)
		return;
	inupdate = 1;
	printf("LINK 1 RX: %09ld TX: %09ld         LINK 2 RX: %09ld TX: %09ld DROPS: %5d (+%5d) QUEUE %3d\r",
			totalfromclient, totaltoclient, totalfromserver, totaltoserver, 
				dropped_packets, get_udp_errors() - initialudperrors, queue_length());
	fflush(stdout);
	inupdate = 0;
}

static int is_same_address(struct sockaddr_storage *a1, struct sockaddr_storage *a2)
{
    if (a1->ss_family != a2->ss_family)
        return 0;
    switch (a1->ss_family) {
    case AF_INET: {
        struct sockaddr_in *a1v4 = (struct sockaddr_in *)a1;
        struct sockaddr_in *a2v4 = (struct sockaddr_in *)a2;
        return a1v4->sin_addr.s_addr == a2v4->sin_addr.s_addr && a1v4->sin_port == a2v4->sin_port;
    }
    case AF_INET6: {
        struct sockaddr_in6 *a1v6 = (struct sockaddr_in6 *)a1;
        struct sockaddr_in6 *a2v6 = (struct sockaddr_in6 *)a2;
        return memcmp((const void *)a1v6->sin6_addr.s6_addr, (const void *)a2v6->sin6_addr.s6_addr, sizeof a1v6->sin6_addr) == 0 && a1v6->sin6_port == a2v6->sin6_port;
    }
    default:
        fprintf(stderr, "Unknown address family\n");
        exit(-1);
    }
}

/* read from server */
void 
do_read_from_server() 
{
	char data[MAXPACKET];
	ssize_t bytes_read;
	int rc;
	static struct sockaddr_storage fromaddr;
	socklen_t fromlen = sizeof fromaddr;

	bytes_read = recvfrom(serversocket, data, sizeof(data), 0, (struct sockaddr*)&fromaddr, &fromlen);
	if (bytes_read == -1 && errno != ECONNREFUSED) {
		perror("error in recv from link 2");
		return;
	}
    if (!is_same_address(&fromaddr, &serveraddr)) {
        char printed_addr[1024];
        int rc = getnameinfo((const struct sockaddr *)&fromaddr, fromlen,
                             printed_addr, sizeof printed_addr, NULL, 0,
                             NI_NUMERICHOST);
        if (rc != 0)
            gai_strerror(rc);

		fprintf(stderr, "ignoring spurious packet from %s\n", printed_addr);
		return;
	}
	totalfromserver += bytes_read;

	rc = sendto(clientsocket, data, bytes_read, 0, (struct sockaddr*)&clientaddr, clientaddrlen);
	if (rc == -1) {
		perror("error in sendto link 1");
	}
	totaltoclient += rc;
}

// we don't use this currently, would be used as user_read_fn in libnet
static void 
read_from_server() 
{
	do_read_from_server();
	update();
}

// thread that receives packets
static void
receiveloop(void *_dummy) 
{
	sigset_t noalarm;
	sigemptyset(&noalarm);
	sigaddset(&noalarm, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &noalarm, 0);

	for (;;) {
		do_read_from_server();
		update();
	}
}

int
create_udp_server_socket(char *localport)
{
    struct addrinfo *info;
    struct addrinfo hint;

    memset(&hint, 0, sizeof hint);
    hint.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
    hint.ai_protocol = IPPROTO_UDP;

    int rc = getaddrinfo(NULL, localport, &hint, &info);
    if (rc != 0)
        gai_strerror(rc), exit(-1);

    // find IPv6 address for dual-bind
    struct addrinfo *pinfo, *use = info;
    for (pinfo = info; pinfo; pinfo = pinfo->ai_next) {
        if (pinfo->ai_family == AF_INET6) {
            use = pinfo;
            break;
        }
    }

	int clientsocket = socket(use->ai_family, use->ai_socktype, use->ai_protocol);
	if (clientsocket == -1) {
		perror("error creating link 1 socket");
		exit(-1);
	}

	rc = bind(clientsocket, use->ai_addr, use->ai_addrlen);
	if (rc == -1) {
		perror("error binding link 1 socket");
		exit(-1);
	}
    freeaddrinfo(info);
    return clientsocket;
}

/* parse optarg after -C, set serveraddr and return localport */
char *
parseCOption(char **av, char *s)
{
	char * localport = strtok(s, ":");
	if (localport == NULL)
		usage("no local port", av[0]);

	serverhost = strtok(NULL, ":");
	if (serverhost == NULL)
		usage("no server host", av[0]);

	serverport = strtok(NULL, ":");
	if (serverport == NULL)
		usage("no remote port", av[0]);

	return localport;
}

static int
create_udp_client_socket(char *serverhost, char *serverport)
{
    struct addrinfo hint;
    struct addrinfo *info;
    memset(&hint, 0, sizeof hint);
    hint.ai_flags = AI_CANONNAME | AI_NUMERICSERV | AI_ADDRCONFIG;
    hint.ai_protocol = IPPROTO_UDP;
    int rc = getaddrinfo(serverhost, serverport, &hint, &info);
    if (rc != 0)
        gai_strerror(rc), exit(-1);

    int serversocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (serversocket == -1) {
		perror("error creating link 2 socket");
		exit(-1);
	}

    serveraddrlen = info->ai_addrlen;
    memcpy(&serveraddr, info->ai_addr, info->ai_addrlen);
	rc = connect(serversocket, info->ai_addr, info->ai_addrlen);
	if (rc == -1) {
		perror("error connecting to link 2");
		exit(-1);
	}
    freeaddrinfo(info);
    return serversocket;
}


int
main(int ac, char *av[]) 
{
	int tx_loss = 0;
	int min_bw = MIN_BW;
	int min_rtt = MIN_RTT;
	int bw_flag = 0;
	int rtt_flag = 0;
	int random_flag = 0;
	char * localport = NULL;
	int dontuse_sendto_flag = 0;
	int c;

	while ((c = getopt(ac, av, "hbvtC:B:D:L:u")) != -1) {
		switch (c) {
			case 'L':
				tx_loss = atoi(optarg); 
				break;
			case 'B':
				min_bw = atoi(optarg); 
				break;
			case 'D':
				min_rtt = atoi(optarg); 
				break;
			case 'b':
				bw_flag = 1; 
				break;
			case 't':
				rtt_flag = 1; 
				break;
			case 'v':
				random_flag = 1; 
				break;
			case 'u':
				dontuse_sendto_flag = 1;
				break;
			case 'C':	
				localport = parseCOption(av, optarg);
				break;
			case 'h':	
				usage("", av[0]); 
			default:
				usage("unknown switch", av[0]); 
		}
	}
	if (localport == 0) {
		usage("must give -C", av[0]); 
	}
	if (optind != ac) {
		usage("extra arguments I don't understand", av[0]); 
	}

	fprintf(stderr, "Parameters for this run:\n");
	fprintf(stderr, "------------------------------------------------------------\n");
	if (dontuse_sendto_flag) {
	    fprintf(stderr, "Operating in direct UDP forwarding mode, NO ARTIFICAL LOSS/DELAY/BW BOTTLENECK IS INTRODUCED.\n");
	} else {
	    fprintf(stderr, "Randomly losing %d out of %d packets, loss probability p=%.4f%s\n", 
			    tx_loss, 1000, tx_loss/1000.0, tx_loss > 0 ? ", see drop.txt" : "");
	    if (bw_flag)
		fprintf(stderr, "Bandwidth is varying between %.3f Kbps and %.3f Kbps"
				", and is changed every %.3f ms, see bw.txt\n", 
				min_bw/1000.0, (min_bw + BW_DEVIATION)/1000.0, BW_CHANGE_INTERVAL/1000.0);
	    else
		fprintf(stderr, "Bandwidth is set to %.3f Kbps\n", min_bw/1000.0);
	    if (rtt_flag)
		fprintf(stderr, "Delay is varying between %.3f milliseconds and %.3f milliseconds"
				", and is changed every %.3f ms, see rtt.txt\n", 
				min_rtt/1000.0, (min_rtt + RTT_DEVIATION)/1000.0, RTT_CHANGE_INTERVAL/1000.0);
	    else
		fprintf(stderr, "Delay is set to %.3f milliseconds\n", min_rtt/1000.0);
	    if (random_flag)
		fprintf(stderr, "I am seeding the pseudo number sequence for loss, bw, and delay variation randomly\n"); 
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "Remember to restart the router for a new run so it can learn the link 1 address (host, port).\n");

	init_net(bw_flag, rtt_flag, random_flag, tx_loss, min_bw, min_rtt);
	atexit(close_net);

	clientsocket = create_udp_server_socket(localport);
    serversocket = create_udp_client_socket(serverhost, serverport);

	if (1) {
		pthread_t t;
		pthread_create(&t, (pthread_attr_t*)NULL, (void *(*)(void *))receiveloop, 0);
	} else {
		register_read(read_from_server, serversocket);
	}
    char printed_addr[1024];
    int rc = getnameinfo((const struct sockaddr *)&serveraddr, serveraddrlen,
                         printed_addr, sizeof printed_addr, NULL, 0, NI_NUMERICHOST);
    if (rc != 0)
        gai_strerror(rc);

	fprintf(stderr, "Link 2 is connected to %s(%s), waiting for incoming packet from Link 1...\n", 
			serverhost, printed_addr);
	initialudperrors = get_udp_errors();

	/* main loop, receives from client and forwards to sender via u_sendto */
	for (;;) {
		char data[MAXPACKET];
		int bytes_read;
		int bytes_sent;
		static int first = 1;

		clientaddrlen = sizeof(clientaddr);
		bytes_read = recvfrom(clientsocket, data, sizeof(data), 0, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (bytes_read == -1) {
			perror("error in recvfrom from link 1");
		}

		if (first && bytes_read > 0) {
            int rc = getnameinfo((const struct sockaddr *)&clientaddr, clientaddrlen,
                                 printed_addr, sizeof printed_addr, NULL, 0, NI_NUMERICHOST);
            if (rc != 0)
                gai_strerror(rc);
			fprintf(stderr, "Learned Link 1 address\n"
					"now forwarding %s <- link 1 -| ",
                        printed_addr);

            rc = getnameinfo((const struct sockaddr *)&serveraddr, serveraddrlen,
                                 printed_addr, sizeof printed_addr, NULL, 0, NI_NUMERICHOST);
            if (rc != 0)
                gai_strerror(rc);

			fprintf(stderr, "router |- link 2 -> %s\n", printed_addr);
			first = 0;
		}
		totalfromclient += bytes_read;

		if (dontuse_sendto_flag) {
			bytes_sent = sendto(serversocket, data, bytes_read, 0, (struct sockaddr*)&serveraddr, serveraddrlen);
			if (bytes_sent == -1)
				perror("sendto to Link 2");
		} else {
			bytes_sent = u_sendto(serversocket, data, bytes_read, 0, (struct sockaddr*)&serveraddr, serveraddrlen);
			if (bytes_sent == BAD_LENGTH) {
				fprintf(stderr, "Packet of length %d dropped because it exceeds MTU %d\n", bytes_read, MTU);
				bytes_sent = 0;
			} else {
				assert(bytes_sent == bytes_read || !!!"this should not happen");
			}
		}

		totaltoserver += bytes_sent;
		update();
	}
}

static unsigned int 
get_udp_errors()
{
	FILE *f = fopen("/proc/net/snmp", "r");
	if (f == NULL) {
		perror("/proc/net/snmp");
		return -1;
	}
	while (!feof(f)) {
		char buf[8192];
		unsigned int a, errors;
		if (4 == fscanf(f, "Udp: %d %d %d %d\n", &a, &a, &errors, &a)) {
			fclose(f);
			return errors;
		}
		fgets(buf, sizeof buf, f);
	}
	fclose(f);
	return -1;
}

