#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#define BACKLOG 10 //queue of max pending requests
#define STDIN 0

struct client
{
	char name[1024];
};
struct client  peer[100]; // ( If only I had std::maps or pair!)
int readmsg(int fd, int BytesToRead, char *InBuff)
{
	int inbytes;
	if((inbytes = recv(fd, InBuff, BytesToRead, 0)) == -1)
	{
		perror("server: read error -- nick"); 
		close(fd);
	}
	if(inbytes==0)
	{
		close(fd);
		printf("%s left\n", peer[fd].name);
	}
	InBuff[inbytes] = 0;
	return inbytes;
}

void send_msg(int fd, char *OutBuff)  //sendmsg is already a function in socket.h
{
	OutBuff[strlen(OutBuff)] = '\0'; //safety is good
	if (send(fd, OutBuff, strlen(OutBuff), 0) == -1)
	{
		perror("server: send error"); 
	}
}

void getnick(int fd, char *clientNick)
{
	char enterNick[] = "Enter a nick (Max 1023 characters) : ";
	int inbytes;
	send_msg(fd, enterNick);
	readmsg(fd, 1023, clientNick);
	clientNick[strlen(clientNick)-1] = '\0'; //last character is newline which is always sent
	return;
}

void *get_in_addr(struct sockaddr_storage *obj)
{
	if (obj->ss_family == AF_INET) 
	{
		return &(((struct sockaddr_in*)obj)->sin_addr);
	}
	return &(((struct sockaddr_in6*)obj)->sin6_addr);
}

void multiplexer(int listener)
{
	socklen_t addrsize;
	struct sockaddr_storage their_addr;
	fd_set readfds, masterfd;
	FD_ZERO(&readfds);
	FD_ZERO(&masterfd);
	FD_SET(listener, &masterfd);
	FD_SET(STDIN, &masterfd);
	int fdmax;
	int inbytes, client_no = -1, new_fd;
	char outmsg[1024], inmsg[1024];
	char incoming_IP[INET6_ADDRSTRLEN]; //Extra size doesn't hurt
	fdmax = listener;
	
	while(1)
	{
		readfds = masterfd;
		if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) 
		{
 			perror("select");
 			exit(4);
 		}
 		for(int i=0; i<=fdmax; i++)
		{	
			if(FD_ISSET(i, &readfds))
			{
				if(i==listener)
				{
					addrsize = sizeof their_addr;
					if((new_fd = accept(listener, (struct sockaddr *)&their_addr, &addrsize)) == -1) //accept  is a blocking function
					{
						perror("server: accept error");
					}
					else
					{
						FD_SET(new_fd, &masterfd); // add to master set
						if (new_fd > fdmax) 
							fdmax = new_fd;
						inet_ntop(their_addr.ss_family, get_in_addr(&their_addr), incoming_IP, INET6_ADDRSTRLEN);
						printf("Server received a connection from %s on socket %d\n",incoming_IP, new_fd);
						getnick(new_fd, peer[new_fd].name);
						char b[] = "Welcome ";
						char *c =  peer[new_fd].name;
						char * dest;
						dest = strcat(b, strcat(c, "\n"));
						send_msg(new_fd, b);
						
					}
				}
				else if(i == 0)
			    {
					if(fgets(outmsg, 1023, stdin)==NULL) //flushes buffer after firing unlike fscanf()
						return ; 
					else
					{	for(int j = 0; j <= fdmax; j++) 
						{
							// send to everyone!
							if (FD_ISSET(j, &masterfd)) 
							{
								// except the listener and ourselves (we typed it!)
								if (j != listener && j != 0 && j!= fileno(stdout) && j!=i) 
								{
									send_msg(j, outmsg);
								}
							}
						}
					}
				}
				else //A client got some data
				{
					if(readmsg(i, 1023, inmsg))
					{
						printf("%s -> %s", peer[new_fd].name, inmsg); //display on server
						sprintf(outmsg, "%s -> %s",peer[new_fd].name, inmsg);
						for(int j = 0; j <= fdmax; j++) 
						{
							// send to everyone!
							if (FD_ISSET(j, &masterfd)) 
							{
								// except the listener and the guy who sent it
								if (j != listener && j != i && j!=0) 
								{
									send_msg(j, outmsg);
								}
							}
						}
					}
					else
						FD_CLR(i, &masterfd);
				}
			}
		}		
	}
}
int main(int argc, char const *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *var;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	int yes = 1;
	int err;
	if(argc != 2)
	{
		fprintf(stderr, "Usage: ./server.out [Port]\n");
	}
	if ((err = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) //servinfo is a linked-list of info. about IPs of the host (us)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); //use stderr to avoid mixing outputs.
		return 1;
	}
	for(var = servinfo; var != NULL; var = var->ai_next) 
	{
		if ((sockfd = socket(var->ai_family, var->ai_socktype, var->ai_protocol)) == -1) 
		{
			perror("server: unable to open socket\n");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
		{
 			perror("setsockopt");
 			exit(1);
 		}
 		if(bind(sockfd, var->ai_addr, var->ai_addrlen) == -1)
 		{
 			perror("server: unable to bind to socket");
 			continue;
 		}

 		break;
 	}
	freeaddrinfo(servinfo); 
	if(var == NULL)
	{
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
	if(listen(sockfd, BACKLOG) == -1)
	{
		perror("server: listen error");
		exit(1);
	}
	printf("Listening for incoming connections..\n");

	


	// Multi-client I/O Multiplexing
	multiplexer(sockfd);
	close(sockfd);
 	return 0; 	
}