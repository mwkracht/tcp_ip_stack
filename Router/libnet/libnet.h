#ifndef __LIBNET_H
#define __LIBNET_H

/* Some error codes returned by u_sendto */
#define BAD_DATA -1
#define BAD_LENGTH -2
#define BUFFER_OVERFLOW -3

/* Maximum length of a data packet in a single call to u_sendto */
#define MTU 500

#define MIN_RTT 150 * 1000              /* Min, one way TT 150 ms */
#define MIN_BW 200 * 1000               /* Min BW is 200 Kbps */
#define RTT_DEVIATION 150 * 1000        /* OTT (one way) deviation over 150 ms */
#define RTT_CHANGE_INTERVAL 750 * 1000  /* RTT changes over 1200 ms intervals */
#define BW_CHANGE_INTERVAL 300 * 1000   /* BW changes over 1 sec intervals */
#define BW_DEVIATION 100 * 1000         /* BW varies over 200 - 300 Kbps */
#define MAX_DATA_QUEUE 200              /* 200 entry packet queue. Blocks after 200 */

/* 
   flags should be 0 or non-zero
   
   bw_flag: 0 = constant bottleneck bw, 1 = variable bottleneck bw
   rtt_flag: 0 = const rtt, 1 = variable rtt
   random_flag: 0 = same random numbers on each run
                1 = random numbers change on each run
   tx_loss: tx loss probability, 0...1000 corresponds to 0.0 to 1.0
   min_bw: bandwidth in bps
   min_rtt: one-way delay in microseconds
*/
void init_net(unsigned char bw_flag, 
	      unsigned char rtt_flag, 
	      unsigned char random_flag,
	      unsigned int tx_loss,
	      unsigned int min_bw,
	      unsigned int min_rtt);
void close_net();

/* 
   Timer functions
   Interval is a 64 bit number.
   Note: System only provides 1 timer. You need to implement your own linked
   list to use multiple timers.

   The minimum timer resolution is 10ms. interval (in us) is the time at which your
   function will be called. 

   The timer function is a one shot. After the timer expires, your function is called
   and the timer is deleted. To create another timer, call the register timer
   function again.
*/
void register_timer(unsigned long long interval, void (*handler)(unsigned long long));
void unregister_timer();

/* 
   Register a read function at the server. This function is called asynchronously if 
   any data is received. You need to pass the socket file handle of the open UDP socket
*/
void register_read(void (*read_fn)(), int fd);


/* Utility Function: Returns current time in us
   Note: Function returns a 64 bit number  */
unsigned long long get_time();


/* Write function. Same parameters as UDP sendto */
int u_sendto(int fd, void *data, int len, int flags, struct sockaddr *to,
	     socklen_t addrlen);

int queue_length();
extern unsigned dropped_packets;
#endif
