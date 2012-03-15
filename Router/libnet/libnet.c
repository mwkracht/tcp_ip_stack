#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include "libnet.h"

typedef struct
{
  unsigned char data[MTU];
  int len;
  int fd;
  unsigned long long tx_time;
  struct sockaddr_storage to;
  socklen_t addrlen;
} qentry;

qentry *qptr = NULL;
volatile int qstart, qend;
unsigned long long last_tx_time;
unsigned int current_rtt, current_bw;
unsigned int min_bw, min_rtt;
unsigned int tx_loss_prob;
unsigned long long last_bw_change, last_rtt_change;
volatile int qlock = 0; /* implements simple semaphore to avoid queue update races */
unsigned char l_bw= 0, l_rtt= 0, l_rel = 0;
unsigned int current_packet = 0;
volatile unsigned long long user_timer_interval;
void (*user_read_fn)() = NULL;
int user_read_fd = 0;
void (*user_timer_fn)(unsigned long long) = NULL;
unsigned long long lstart_time;
unsigned dropped_packets;
FILE *bw_fptr, *rtt_fptr, *drop_fptr;

static int enqueue_data(int fd, void *data, int len, struct sockaddr *to, socklen_t addrlen);
static qentry *dequeue_data(unsigned long long current_time);
static void timer();

/*
static int
getenv_integer(char *env_var, int default_value) {
	char *s = getenv(env_var);
	if (s)
		return atoi(s);
	else
		return default_value;
}

static int
isBlocked(int signal)
{
	sigset_t new, old;
	sigemptyset(&new);
	sigprocmask(SIG_BLOCK, &new, &old);
	return sigismember(&old, signal);
}

*/

void init_net(unsigned char bw_flag, unsigned char rtt_flag, 
	      unsigned char random_flag,
	      unsigned int tx_loss,
	      unsigned int _min_bw,
	      unsigned int _min_rtt)
{
  struct itimerval itimer;
  
  l_bw = bw_flag;
  l_rtt = rtt_flag;
  l_rel = tx_loss > 0;

  qptr = (qentry *) malloc(MAX_DATA_QUEUE * sizeof(qentry));
  if (!qptr)
    {
      printf("libnet: Unable to allocate internal transmission buffers. Aborting ...\n");
      exit(1);
    }

  qstart = qend = 0;
  if (random_flag) srandom(get_time());
  lstart_time = get_time();
  last_tx_time = lstart_time;

  bw_fptr = fopen("bw.txt", "w+");
  drop_fptr = fopen("drop.txt", "w+");
  rtt_fptr = fopen("rtt.txt", "w+");
  if (!bw_fptr || !drop_fptr || !rtt_fptr)
    {
      printf("Unable to open result graph files. Aborting ...\n");
      exit(1);
    }

  current_rtt = min_rtt = _min_rtt;
  current_bw = min_bw = _min_bw;
  tx_loss_prob = tx_loss;

  fprintf(bw_fptr, "0 %u\n", current_bw);
  fprintf(rtt_fptr, "0 %u\n", current_rtt/1000);
  
  if (0) {
	  signal(SIGALRM, timer);
  } else {
	  struct sigaction sa;
	  sa.sa_handler = timer;
	  sigemptyset(&sa.sa_mask);
	  sa.sa_flags = SA_RESTART;
	  sigaction(SIGALRM, &sa, 0);
  }
  itimer.it_interval.tv_sec = 0;
  itimer.it_interval.tv_usec = 1000; /* 1 ms timer */
  itimer.it_value.tv_sec = 0;
  itimer.it_value.tv_usec = 1000;
  setitimer(ITIMER_REAL, &itimer, NULL);

  fflush(bw_fptr);
  fflush(rtt_fptr);
  fflush(drop_fptr);
}

void close_net()
{
  qentry *ptr;
  volatile unsigned long long current_time;
  struct itimerval itimer;

   /* Disable timer */
  user_read_fn = NULL;
  itimer.it_interval.tv_sec = 0;
  itimer.it_interval.tv_usec = 0; /* 1 ms timer */
  itimer.it_value.tv_sec = 0;
  itimer.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &itimer, NULL);

  /* empty any remaining queue entries */
  while (1)
    { 
      if (qstart == qend) break;
      current_time = get_time();
      ptr = dequeue_data(current_time);
      if (ptr ==  NULL) continue; /* Maybe time hasn't passed yet */
      sendto(ptr->fd, ptr->data, ptr->len, 0, (struct sockaddr *)&(ptr->to), ptr->addrlen);
      free(ptr->data);
    } 

  
  fclose(bw_fptr);
  fclose(rtt_fptr);
  fclose(drop_fptr);
}


void register_timer(unsigned long long interval, void (*handler)(unsigned long long))
{
  user_timer_fn = handler;
  user_timer_interval = interval;
}


void register_read(void (*read_fn)(), int fd)
{
  // printf("Read fn registered. Registering FD %d\n", fd);
  user_read_fn = read_fn;
  user_read_fd = fd;
}
  
void unregister_timer()
{
  user_timer_fn = NULL;
}


int u_sendto(int fd, void *data, int len, int flags, struct sockaddr *to, 
	     socklen_t addrlen)
{
  if (len < 0 || data == NULL) return(BAD_DATA);
  if (len == 0) return(0);
  if (len > MTU) { dropped_packets++; return(BAD_LENGTH); }

  enqueue_data(fd, data, len, to, addrlen);
  return(len);
}

static
int enqueue_data(int fd, void *data, int len, struct sockaddr *to, socklen_t addrlen)
{
  int p_loss;
  int drop_len;
  unsigned long long current_time;

  /* Block on data transmission if the queues are full.
     The timer will empty them */

  while (((qend + 1) % MAX_DATA_QUEUE) == qstart) ; 
  
  while (qlock);
  qlock = 1;

  current_packet++;
  
  current_time = get_time();
  if (l_rel)
    {
      p_loss = random() % 1000;
      if (p_loss < tx_loss_prob) 
	{
	  /* Transmit part of the packet before dropping it */
	  fprintf(drop_fptr, "%llu %u\n", (current_time-lstart_time)/1000, 
		  current_packet);
	  if (current_time > last_tx_time) last_tx_time = current_time;
	  drop_len = random() % len;
	  last_tx_time += 
	    (int)(drop_len*8*1000000.0/(double)current_bw);
	  
	  /* Block while we transmit the packet */
	  while (get_time() < last_tx_time) ;

	  dropped_packets++;
	  fflush(drop_fptr);
	  qlock = 0;
	  return(1);
	}
    }
  
#if 0
  /* can't do that because we can't call free in signal handler! */
  qptr[qend].data = (unsigned char *) malloc(len);
  if (!qptr[qend].data)
    {
      printf("libnet: Unable to allocate internal transmission buffers.\n");
      fprintf(drop_fptr, "%llu %u\n", (current_time-lstart_time)/1000, 
	      current_packet);
      if (current_time > last_tx_time) last_tx_time = current_time;
      drop_len = random() % len;
      last_tx_time += 
	(drop_len*8*(int)((double)(1000000.0/(double)current_bw)));
      fflush(drop_fptr);
      qlock = 0;
      return(1);
    }
#endif

  memcpy(qptr[qend].data, data, len);
  qptr[qend].len = len;
  qptr[qend].fd = fd;
  memcpy(&qptr[qend].to, to, addrlen);
  qptr[qend].addrlen = addrlen;
  
  
  /* Change data rate to bandwidth bottleneck rate */
  if (current_time > last_tx_time) last_tx_time = current_time;
  // WAS:
  // last_tx_time += (len*8*(int)((double)(1000000.0/(double)current_bw)));
  // which works poorly if current_bw is larger than .25 Mbps
  // Note: we have 1ms accuracy, which is 1000 packet/s = 500KB/s = 4Mbps
  last_tx_time += (int)(len*8*1000000.0/(double)current_bw);

  /* Block while we transmit the packet */
  while (get_time() < last_tx_time) ;
  
  /* Now queue it to add RTT effect */
  qptr[qend].tx_time = get_time() + current_rtt;

  qend = ((qend + 1) % MAX_DATA_QUEUE);

  qlock = 0;
  return(1);
}

static qentry *dequeue_data(unsigned long long current_time)
{
  qentry *ptr;

  if (qstart == qend) return(NULL); /* Empty queue */

  if (qptr[qstart].tx_time <= current_time) 
    {
      ptr = &(qptr[qstart]);
      qstart = (qstart + 1) % MAX_DATA_QUEUE;
      return(ptr);
    }
  else return(NULL);
}

int
queue_length()
{
  int l = qend - qstart;
  return l >= 0 ? l : MAX_DATA_QUEUE + l;
}

static void 
timer()
{
  unsigned long long current_time;
  qentry *ptr;
  static int tlock = 0;

  assert(tlock == 0 || !!!"Signal handler reentered - this must not happen.");
  tlock = 1;
  
  /* qlock is immaterial here. We should be able to do a real 
     phy transmit even if u_sendto is blocked, else we'll have RTT
     errors */
  while (1)
    {
      current_time = get_time();
      ptr = dequeue_data(current_time);
      if (ptr ==  NULL) break;
      sendto(ptr->fd, ptr->data, ptr->len, 0, &(ptr->to), ptr->addrlen);
      // data is preallocated now because this would be illegal:
      // free(ptr->data);
    }

  if (l_rtt && current_time - last_rtt_change > RTT_CHANGE_INTERVAL)
    {
      current_rtt = (random() % RTT_DEVIATION) + min_rtt;
      last_rtt_change = current_time;
      fprintf(rtt_fptr, "%llu %u\n", (current_time-lstart_time)/1000, 
	      current_rtt/1000);
      fflush(rtt_fptr);
    }

  if (l_bw && current_time - last_bw_change > BW_CHANGE_INTERVAL)
    {
      current_bw = (random() % BW_DEVIATION) + min_bw;
      last_bw_change = current_time;
      fprintf(bw_fptr, "%llu %u\n", (current_time-lstart_time)/1000,
	      current_bw);
      fflush(bw_fptr);
    }

  /* Blocked in transmit call. Don't call user
     timer since it may try to transmit and cause
     a race. Similar justification for not 
     calling user read fn. It may transmit and
     cause a race */
  if (qlock) 
    { 
      goto leave;
    }

  if (user_timer_fn != NULL && current_time >= user_timer_interval) 
    {
      unsigned long long last_timer = user_timer_interval;
      (*user_timer_fn)(current_time);
      if (user_timer_interval == last_timer) user_timer_fn = NULL;
    }

  if (user_read_fn != NULL)
    {
      fd_set rfds;
      struct timeval tv;
      int retval;

      while (1)
	{
	  FD_ZERO(&rfds);
	  FD_SET(user_read_fd, &rfds);

	  /* Don't wait. */
	  tv.tv_sec = 0;
	  tv.tv_usec = 0;
	  
	  retval = select(user_read_fd+1, &rfds, NULL, NULL, &tv);
	  if (retval) (*user_read_fn)();
	  else break;
	}
    }
leave:
  tlock = 0;
}

unsigned long long get_time()
{
  struct timeval time_val;
  struct timezone time_zone;
  static unsigned long long last_time = 0ll;
  unsigned long long current_time;
  
  current_time = 0ll;
  gettimeofday(&time_val, &time_zone);
  current_time = (unsigned long long) time_val.tv_sec*1000000 + 
    (unsigned long long) time_val.tv_usec;
  
  if (current_time < last_time) return(last_time);

  last_time = current_time;
  return(current_time);
}


