/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 
#define BILLION 1000000000.0


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[BUFSIZ + 1];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char *filename;

	struct timespec start, end; 

	if (argc != 3) 
	{
	    fprintf(stderr, "usage: client hostname filename\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	rv = getaddrinfo(argv[1], PORT, &hints, &servinfo);
	if (rv != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) 
	{
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == -1) 
		{
			perror("	socket failed");
			continue;
		}

		rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
		if (rv == -1) 
		{
			perror("	connect failed");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) 
	{
		fprintf(stderr, "	failed to connect\n");
		return 2;
	} 

	inet_ntop(p->ai_family, 
					  get_in_addr((struct sockaddr *)p->ai_addr), 
						s, 
				    sizeof s);
	printf("	connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	clock_gettime(CLOCK_MONOTONIC, &start);

	char *message = "GET\n";

	// Send server a "GET\n message"
	rv = send(sockfd, &message, sizeof message, 0);
	if (rv == -1) 
	{
		perror("	send() failed");
		close(sockfd);
		exit(1);
	}

	// Open file for writing recieved data to
	FILE *recieved_fd = fopen(argv[2], "wb");
	if (recieved_fd == NULL)
	{
		perror("	Open() failed.");
		close(sockfd);
		exit(1);
	}

	// Recieve the file
	while ((numbytes = recv(sockfd, buf, BUFSIZ, 0)) > 0) 
	{	
		fwrite(buf, 1, numbytes, recieved_fd);
	}
	
	if (numbytes < 0) 
	{
		perror("	recv() failed");
		fclose(recieved_fd);
		close(sockfd);
		exit(1);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);

	printf("	pollserver: socket %d hung up\n", sockfd);

	fclose(recieved_fd);
	
  double time_taken = (end.tv_sec - start.tv_sec) + 
  										(end.tv_nsec - start.tv_nsec) / BILLION;

	printf("	time elapsed is %f seconds\n", time_taken);

	close(sockfd);

	return 0;
}
