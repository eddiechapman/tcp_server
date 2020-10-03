#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *connection_handler(void *socket_desc)
{
  int sock = *(int*)socket_desc;
  int bytes_read, bytes_sent;
  char buf[BUFSIZ];

  bytes_read = recv(sock, buf, sizeof(buf), 0);
  
  if (bytes_read < 0)
    perror("  recv() failed");
  
  else if (bytes_read == 0)
    printf("  pollserver: socket %d hung up\n", sock);
  
  printf("  File has been requested\n");

  FILE * fp = fopen("example.txt", "rb");
  
  if (fp == NULL)
    perror("  open() failed");

  while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0)
  {
    bytes_sent = send(sock, buf, bytes_read, 0);
    if (bytes_sent < 0)
    {
      perror("  send() failed");
      fclose(fp);
    }
  }

  close(sock);
    
  printf("  File delivered to socket %d\n", sock);

  pthread_exit(NULL);
}

void *connection_handler(void *);

int main(void)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, " failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("  waiting for connections...\n");

  pthread_t thread_id;

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("  accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("  got connection from %s\n", s);

    rv = pthread_create(&thread_id, NULL, 
                        connection_handler, 
                        (void*) &new_fd);
    if (rv < 0)
    {
      perror("  pthread_create() failed");
      continue;
    }

	}
  
  pthread_exit(NULL);
	return 0;
}
