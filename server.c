#include        <sys/types.h>   /* basic system data types */
#include        <sys/socket.h>  /* basic socket definitions */
#include        <sys/time.h>    /* timeval{} for select() */
#include        <time.h>                /* timespec{} for pselect() */
#include        <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include        <arpa/inet.h>   /* inet(3) functions */
#include        <errno.h>
#include        <fcntl.h>               /* for nonblocking */
#include        <netdb.h>
#include        <signal.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include 		<unistd.h>
#include		<wait.h>
#include 		<syslog.h>
//moje
//#include <cstdlib.h>
#include <string.h>

#define MAXLINE 1024

#define SA struct sockaddr

#define LISTENQ 2

#define MULTICAST_IP "224.0.0.1"
#define MULTICAST_PORT 1234
#define MAXFD 64

int los;
int count = 0;

int
daemon_init(const char *pname, int facility, uid_t uid, int socket1, int socket2)
{
	int		i, p;
	pid_t	pid;

	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		exit(0);			/* parent terminates */

	/* child 1 continues... */

	if (setsid() < 0)			/* become session leader */
		return (-1);

	signal(SIGHUP, SIG_IGN);
	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		exit(0);			/* child 1 terminates */

	/* child 2 continues... */

	chdir("/tmp");				/* change working directory  or chroot()*/
//	chroot("/tmp");

	/* close off file descriptors */
	for (i = 0; i < MAXFD; i++){
		if((socket1 != i) && (socket2 != i) )
			close(i);
	}

	/* redirect stdin, stdout, and stderr to /dev/null */
	p= open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	openlog(pname, LOG_PID, facility);
	
        syslog(LOG_ERR," STDIN =   %i\n", p);
	setuid(uid); /* change user */
	
	return (0);				/* success */
}

void
sig_chld(int signo)
{
	pid_t	pid;
	int		stat;

	while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
		syslog(LOG_INFO,"child %d terminated\n", pid);
	return;
}

void
sig_pipe(int signo)
{
	syslog(LOG_NOTICE, "Server received SIGPIPE - Default action is exit \n");
	exit(1);
}

ssize_t						/* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;		/* and call write() again */
			else
				return(-1);			/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n);
}
/* end writen */

void
Writen(int fd, void *ptr, size_t nbytes)
{
	if (writen(fd, ptr, nbytes) != nbytes)
		syslog(LOG_ERR, "writen error: %s",strerror(errno));
		// perror("writen error");
}

void
str_echo(int sockfd, int sockfd_multi, struct sockaddr_in multicast_addr, char addr_buf[INET6_ADDRSTRLEN+1])
{
	ssize_t		n;
	char		buf[MAXLINE], send_buf[MAXLINE];
	int i = 0;
	socklen_t addr_len;

	if(count==1) {
		write(sockfd,"wait",4);
	}else {
		write(sockfd,"start",5);
	}
	
again:
	while ( (n = read(sockfd, buf, MAXLINE)) > 0){
		
		if(!(atoi(buf) == los)){
			static char msg1[MAXLINE] = "Nie zgadles :(\n";
			Writen(sockfd, msg1, MAXLINE);
		}else{
			static char msg2[MAXLINE] = "Zgadles liczbe! \n";
			Writen(sockfd, msg2, MAXLINE);
			sprintf(send_buf,"Player %s guessed the number\n",addr_buf);
			if(sendto(sockfd_multi, send_buf,INET6_ADDRSTRLEN+1,0,(struct sockaddr *) &multicast_addr, sizeof(multicast_addr)) < 0) {
				syslog(LOG_ERR,"Sendto error");
				exit(1);
			}
		}
		
		
	}
	if (n < 0 && errno == EINTR)
		goto again;
	else if (n < 0)
		syslog(LOG_ERR, "str_echo: read error");
}
			

int
main(int argc, char **argv)
{
	srand(time(NULL));

	int					listenfd, connfd;
	pid_t				childpid;
	socklen_t			clilen;
	struct sockaddr_in6	cliaddr, servaddr;
	void				sig_chld(int);
	char 				addr_buf[INET6_ADDRSTRLEN+1];
	
	int sockfd_multi;
    struct sockaddr_in multicast_addr;


    // Create a socket
    if ( (sockfd_multi = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
          fprintf(stderr,"socket error : %s\n", strerror(errno));
          return 1;
   }

    // Set up the multicast address struct
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
    multicast_addr.sin_port = htons(MULTICAST_PORT);

   if ( (listenfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0){
          fprintf(stderr,"socket error : %s\n", strerror(errno));
          return 1;
   }


	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin6_family = AF_INET6;
	servaddr.sin6_addr   = in6addr_any;
	servaddr.sin6_port   = htons(7);	/* echo server */

   if ( bind( listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
           fprintf(stderr,"bind error : %s\n", strerror(errno));
           return 1;
   }

   if ( listen(listenfd, LISTENQ) < 0){
           fprintf(stderr,"listen error : %s\n", strerror(errno));
           return 1;
   }

	daemon_init("tcp_random_number", LOG_USER, 1000, listenfd, sockfd_multi);

	syslog (LOG_NOTICE, "Program started by User %d", getuid ());
	syslog (LOG_INFO,"Waiting for clients ... ");

	signal(SIGCHLD, sig_chld);
	signal(SIGPIPE, sig_pipe);

	for ( ; ; ) {

		clilen = sizeof(cliaddr);
		if ( (connfd = accept(listenfd, (SA *) &cliaddr, &clilen)) < 0) {
			if (errno == EINTR)
				continue;		/* back to for() */
			else
				syslog(LOG_ERR,"accept error : %s\n", strerror(errno));
				exit(1);
		}
		count++;
		if(count==2)
		{	
			los = rand()%10;
			if (sendto(sockfd_multi, "Starting game\n Guess the number from 0-9\n", 44, 0, (struct sockaddr *) &multicast_addr, sizeof(multicast_addr)) < 0) {
				syslog(LOG_ERR, "Sendto error");
				exit(1);
			}
			count = 0;
		}
			bzero(addr_buf, sizeof(addr_buf));
			inet_ntop(AF_INET6, (struct sockaddr  *) &cliaddr.sin6_addr,  addr_buf, sizeof(addr_buf));
			syslog(LOG_INFO, "New client: %s, port %d\n", addr_buf, ntohs(cliaddr.sin6_port));


		if ( (childpid = fork()) == 0) {	/* child process */
		
			close(listenfd);	/* close listening socket */
			str_echo(connfd, sockfd_multi, multicast_addr, addr_buf);	/* process the request */
			exit(0);
		}
		close(connfd);			/* parent closes connected socket */
	}
}
