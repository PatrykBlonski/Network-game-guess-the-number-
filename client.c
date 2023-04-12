//#include	"unp.h"
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
#include		<math.h>

#define MAXLINE 1024
#define SA      struct sockaddr
#define MULTICAST_IP "224.0.0.1"
#define MULTICAST_PORT 1234

ssize_t
Read(int fd, void *ptr, size_t nbytes)
{
	ssize_t		n;

	if ( (n = read(fd, ptr, nbytes)) == -1){
			perror("read error");
//			exit(1);
	}
	return(n);
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
		perror("writen error");
}

void
str_cli(FILE *fp, int sockfd, int sockfd_multi, struct sockaddr_in multicast_addr)
{
	char	sendline[MAXLINE], recvline[MAXLINE], recvline_multi[MAXLINE];
	int n;
	socklen_t addr_len = sizeof(multicast_addr);
	fd_set readfds;
	int maxfdp, stdineof=0;
	
	FD_ZERO(&readfds);
	
	
	printf("\n>");


	while (1) {
		if (stdineof == 0){
			FD_SET(fileno(fp), &readfds);
			fflush(stdout);
		}

		FD_SET(sockfd_multi, &readfds);
		FD_SET(sockfd,&readfds);
		maxfdp=fmax(fileno(fp),sockfd_multi);
		maxfdp=fmax(maxfdp,sockfd) + 1;
		if(select(maxfdp,&readfds,NULL,NULL,NULL) < 0) {
			perror("select error");
			exit(1);
		}
		if(FD_ISSET(sockfd,&readfds)) {
			if ((n=read(sockfd, recvline, MAXLINE)) == 0){
				if(stdineof == 1)
					return;
				else {
					perror("str_cli: server terminated prematurely");
					exit(0);
				}
			}
			recvline[n]=0;
			printf("%s",recvline);
			fflush(stdout);
			printf("\n>");
		}
		
		if(FD_ISSET(sockfd_multi,&readfds)) {
			if(recvfrom(sockfd_multi,recvline_multi,MAXLINE,0, (struct sockaddr *) &multicast_addr,&addr_len) < 0) {
				fprintf(stderr,"recv error : %s\n", strerror(errno));
				exit(0);
			}
			if(strncmp(recvline_multi,"Player",6)==0){
				printf("%s",recvline_multi);
				return;
			}
		}
		if (FD_ISSET(fileno(fp), &readfds)) {  /* input is readable */
			if ( (n = Read(fileno(fp), sendline, MAXLINE)) == 0) {
				stdineof = 1;

				if (shutdown(sockfd, SHUT_WR) < 0){
					perror("shutdown error");
					exit(1);				
				}
				FD_CLR(fileno(fp), &readfds);
				continue;
			}
			Writen(sockfd, sendline, strlen(sendline));		
		}
	}

	return;
}


int
main(int argc, char **argv)
{
	int					sockfd, n;
	struct sockaddr_in6	servaddr;
	char				recvline[MAXLINE + 1];
	
	int sockfd_multi;
    struct sockaddr_in multicast_addr;
    socklen_t addr_len;
	struct ip_mreq multicast_req;

    // Create a socket
    
	if (argc != 2){
		fprintf(stderr, "usage: %s <IPaddress> \n", argv[0]);
		return 1;
	}

	if ( (sockfd_multi = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
          fprintf(stderr,"socket error : %s\n", strerror(errno));
          return 1;
    }

    // Set up the multicast address struct
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_IP);
    multicast_addr.sin_port = htons(MULTICAST_PORT);

	if(bind(sockfd_multi, (struct sockaddr *) &multicast_addr, sizeof(multicast_addr)) < 0) {
		fprintf(stderr,"bind error : %s\n", strerror(errno));
        return 1;
	}

	multicast_req.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
	multicast_req.imr_interface.s_addr = htonl(INADDR_ANY);
	if(setsockopt(sockfd_multi, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multicast_req, sizeof(multicast_req)) < 0) {
		fprintf(stderr,"multicast group error : %s\n", strerror(errno));
        return 1;
	}
	

	if ( (sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0){
		fprintf(stderr,"socket error : %s\n", strerror(errno));
		return 1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin6_family = AF_INET6;
	servaddr.sin6_port   = htons(7);	/* echo server */
	if (inet_pton(AF_INET6, argv[1], &servaddr.sin6_addr) <= 0){
		fprintf(stderr,"Address error: inet_pton error for %s : %s \n", argv[1], strerror(errno));
		return 1;
	}
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr,"connect error : %s \n", strerror(errno));
		return 1;
	}

	bzero(recvline,MAXLINE+1);
	if(read(sockfd, recvline,MAXLINE) == 0){
		perror("read error");
		exit(0);
	}
	if(strcmp(recvline,"wait") == 0){
		printf("Waiting for second client\n");
		fflush(stdout);
	}
	

	addr_len=sizeof(multicast_addr);
	while(1){
	if(recvfrom(sockfd_multi, recvline, MAXLINE, 0, (struct sockaddr *) &multicast_addr, &addr_len) < 0){
			fprintf(stderr,"recv error : %s\n", strerror(errno));
    	    exit(0);
   	}
	if(strncmp(recvline,"Starting",8)==0){
		printf("%s",recvline);
		break;
	}
	}

	

	str_cli(stdin, sockfd, sockfd_multi, multicast_addr);		/* do it all */
	exit(0);
}
