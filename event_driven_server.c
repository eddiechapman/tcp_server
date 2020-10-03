/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

#define PORT "3490"  // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold

/***************************************************************/
/* get_in_addr - get sockaddr, IPv4 or IPv6                    */
/***************************************************************/
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/***************************************************************/
/* get_listener_socket                                         */
/***************************************************************/
int get_listener_socket(void)
{
	int listener;
	int yes = 1;
	int rv;

	struct addrinfo hints, *ai, *p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	/*************************************************************/
  /* Get information about our host name.                      */
  /*************************************************************/
	rv = getaddrinfo(NULL, PORT, &hints, &ai);
	if (rv != 0) 
	{
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}

	/*************************************************************/
  /* Loop through all the results in our address information   */
	/* and attempt to connect to one                             */
  /*************************************************************/
	for (p = ai; p != NULL; p = p->ai_next) 
	{
		/*************************************************************/
  	/* Create a stream socket descriptor to receive incoming     */
  	/* connections on                                            */
  	/*************************************************************/
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) 
		{
			perror("socket() failed");
			continue;
		}
		/*************************************************************/
  	/* Allow socket descriptor to be reuseable                   */
  	/*************************************************************/
		rv = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (rv < 0)
		{
			perror("setsockopt() failed");
			close(listener);
			continue;
		}

		/*************************************************************/
  	/* Set the socket to be nonblocking.				                 */
  	/*************************************************************/
		rv = fcntl(listener, F_SETFL, O_NONBLOCK);
		if (rv < 0)
		{
			perror("fcntl() failed");
			close(listener);
			continue;
		}

		/*************************************************************/
  	/* Bind the socket.													                 */
  	/*************************************************************/
		rv = bind(listener, p->ai_addr, p->ai_addrlen);
		if (rv < 0) 
		{
			perror("bind() failed");
			close(listener);
			continue;
		}

		/*************************************************************/
  	/* Our socket has been set up successfully.	Exit loop.       */
  	/*************************************************************/
		break;
	}

	/*************************************************************/
  /* If we didn't get a working socket out of the above loop,  */
	/* we're out of luck.																	       */
  /*************************************************************/
	if (p == NULL) 
		return -1;

	/*************************************************************/
  /* Free memory from the address info structure now that we   */
	/* have a working connection.													       */
  /*************************************************************/
	freeaddrinfo(ai);  // All done with this

	/*************************************************************/
  /* Tell the socket to listen for incoming connections.       */
  /*************************************************************/
	rv = listen(listener, BACKLOG);
	if (rv < 0) 
		return -1;

	return listener;
}



int main(void)
{
	int listener;				// Listening socket descriptor

	int new_fd;					// Newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr;			// Client address
	socklen_t addrlen;;

	char buf[BUFSIZ];		// Buffer for client data
	int fd; 						// File descriptor for requested file

	char remoteIP[INET6_ADDRSTRLEN];

	int fd_count = 1;		// Number of connected sockets (1 for listener)
	int fd_size = BACKLOG;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);
	int *sent = malloc(fd_size * sizeof(int));

	/*************************************************************/
	/* Set up and get a listening socket 												 */
	/*************************************************************/
	listener = get_listener_socket();
	if (listener == -1) 
	{
		fprintf(stderr, "error getting listening socket\n");
		exit(1);
	}

	/*************************************************************/
	/* Add the listener to file descriptor set. 								 */
	/*************************************************************/
	pfds[0].fd = listener;
	pfds[0].events = POLLIN;  // Notify upon ready-to-read events

	/*************************************************************/
  /* Loop waiting for incoming connections or for incoming     */
	/* data on any of the connected sockets.                     */
  /*************************************************************/
	do
	{
		int poll_count = poll(pfds, fd_count, -1);  // Infinite timeout
		if (poll_count == -1) 
		{
			perror("poll() failed");
			exit(1);
		}
		
		/***********************************************************/
    /* One or more descriptors have new events. Which ones?    */
    /***********************************************************/
		for(int i = 0; i < fd_count; i++) 
		{
			
			/*********************************************************/
      /* Skip descriptors without a new event status.		     */
      /*********************************************************/
			if (pfds[i].revents == 0)
				continue; 

			
			printf("\n  fd=%d; events: %s%s%s%s\n", 
					   pfds[i].fd,
						 (pfds[i].revents & POLLIN)  ? "POLLIN "  : "",
						 (pfds[i].revents & POLLOUT)  ? "POLLOUT "  : "",
						 (pfds[i].revents & POLLHUP) ? "POLLHUP " : "",
             (pfds[i].revents & POLLERR) ? "POLLERR " : "");
			
			if (pfds[i].fd == listener)
			{
				/*******************************************************/
        /* Listening descriptor is readable.                   */
        /*******************************************************/
        printf("  Listening socket is readable\n");

				/*******************************************************/
        /* Accept all incoming connections queued up on the    */
				/* listening socket before we loop back and call poll  */
				/* again.														                   */
        /*******************************************************/
				do 
				{
					/*****************************************************/
          /* Accept each incoming connection. If               */
          /* accept fails with EWOULDBLOCK, then we            */
          /* have accepted all of them. Any other              */
          /* failure on accept will cause us to end the        */
          /* server.                                           */
          /*****************************************************/
					addrlen = sizeof remoteaddr;
					new_fd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
					if (new_fd < 0) 
					{
						if (errno != EWOULDBLOCK)
						{
							perror("  accept() failed");
							// end server?
						}
						break;
					} 
				
					/*******************************************************/
					/* Add incoming connection to pollfd.                  */
					/*******************************************************/
					// Check for room
					if (fd_count == fd_size) 
					{
						fd_size *= 2; // double it
						pfds = realloc(pfds, sizeof(pfds) * fd_size);
						sent = realloc(sent, sizeof(sent) * fd_size);
					}

					pfds[fd_count].fd = new_fd;

					/*************************************************************/
					/* Check for ready-to-read, ready-to-write events            */
					/*************************************************************/
					pfds[fd_count].events = POLLIN | POLLOUT;

					fd_count++;

					printf("	pollserver: new connection from %s on socket %d\n", 
								 inet_ntop(remoteaddr.ss_family, 
								 					 get_in_addr((struct sockaddr*)&remoteaddr), 
													 remoteIP, INET6_ADDRSTRLEN), 
								 new_fd);

					sent[i] = 0;
					
					/*****************************************************/
          /* Loop back up and accept another incoming          */
          /* connection                                        */
          /*****************************************************/
				} while (new_fd != -1);

			}

			/*********************************************************/
      /* This is not the listening socket, therefore an        */
      /* existing connection must be readable or writable.     */
      /*********************************************************/
			else
			{
					
				if (pfds[i].revents & POLLIN)
				{
					/*****************************************************/
          /* Connection is readable                            */
          /*****************************************************/

					/*****************************************************/
					/* Receive data on this connection until the         */
					/* recv fails with EWOULDBLOCK.                                       */
					/*****************************************************/
					int nbytes = recv(pfds[i].fd, buf, sizeof(buf), 0);
					if (nbytes < 0) 
					{
						if (errno != EWOULDBLOCK)
						{
							perror("  recv() failed");
							close(pfds[i].fd);
							pfds[i] = pfds[fd_count - 1];
							sent[i] = sent[fd_count - 1];
							fd_count--;
						}	
						break;
					}

					/*****************************************************/
					/* Check to see if the connection has been           */
					/* closed by the client                              */
					/*****************************************************/
					if (nbytes == 0) 
					{
						printf("	pollserver: socket %d hung up\n", pfds[i].fd);
						close(pfds[i].fd);
						pfds[i] = pfds[fd_count - 1];
						sent[i] = sent[fd_count - 1];
						fd_count--;
						break;
					}

					buf[nbytes] = '\0';

					/*****************************************************/
					/* The client has requested a file.                  */
					/*****************************************************/
					printf("	File has been requested.\n");

				}
				
				else if (pfds[i].revents & POLLOUT)
				{
					/*****************************************************/
          /* Connection is writeable.                          */
          /*****************************************************/

					// Check if the position structure has an entry for this fd
					int prog = sent[i];

					FILE * disk_fd = fopen("example.txt", "rb");
					if (disk_fd == NULL)
      		{
        		perror("	open() failed");
						close(pfds[i].fd);
						pfds[i] = pfds[fd_count - 1];
						sent[i] = sent[fd_count - 1];
						fd_count--;
						break;
      		}

					// printf("	open() successful.");

					fseek(disk_fd, 0, SEEK_END);
					int file_size = ftell(disk_fd);

					// printf("	File size: %d	bytes\n", size);

					// Seek up to the point specified in the position structure
					int rv = fseek(disk_fd, prog, SEEK_SET);
					if (rv != 0)
					{
						fclose(disk_fd);
						perror("	Server: fseek()  failed");
					}

					printf("	File position before send: %ld\n", ftell(disk_fd));
					
					// Read the next chunk into the buffer
					fread(buf, 1, sizeof(buf), disk_fd);
					if (buf == NULL)
					{
						perror("	Server: fread() failed");
						fclose(disk_fd);
					}

					// printf("	Current buffer contents: %s\n", buf);

					printf("	File position after send: %ld\n", ftell(disk_fd));

					int send_amount = sizeof(buf);
					if (file_size - prog < sizeof(buf))
						send_amount = file_size - prog;

					// Send the next chunk
					int num_bytes = send(pfds[i].fd, buf, send_amount, 0);
					if (num_bytes == -1)
					{
						perror("	Server: send() failed");
						fclose(disk_fd);

					}
					
					printf("	Server: sent %d bytes from position %d\n", num_bytes, prog);
					// printf("	Sent buffer contents: %s\n", buf);


					if (feof(disk_fd))
					{
						printf("	We have reached the end of the file.\n");
						fclose(disk_fd);
						close(pfds[i].fd);
						pfds[i] = pfds[fd_count - 1];
						sent[i] = sent[fd_count - 1];
						fd_count--;
					}
					else
					{
						sent[i] = prog + num_bytes;
						fclose(disk_fd);
					}
					
				}

				else
				{
					printf("  Error! revents = %d\n", pfds[i].revents);
					exit(1);
				}



			}  /* End of client socket */
				
		}  /* End of cycling through descriptors */
	
	} while (1); /* End of polling sockets */

	return 0;
}

