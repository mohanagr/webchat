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
#define MAX_CLIENTS 10 

static int uid = 0;

/* I miss C++ STL */

typedef struct {	
	int connfd;			/* Connection file descriptor */
	int uid;			/* Client unique identifier */
	char name[32];			/* Client name */
} client_t;
client_t *clients[MAX_CLIENTS] = {NULL};

void queue_add(client_t *cl)
{
	int i;
	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(!clients[i])
		{
			clients[i] = cl;
			return;
		}
	}
}

void queue_delete(int id)
{
	int i;
	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(clients[i])
		{
			if(clients[i]->connfd == id)
			{
				clients[i] = NULL;
				return;
			}
		}
	}
}

int get_index(int fd)
{
	int i;
	for(i=0;i<MAX_CLIENTS;i++)
	{
		if(clients[i])
		{
			if(clients[i]->connfd == fd)
			{
				return i ;
			}
		}
	}
}

int readmsg(int fd, int BytesToRead, char *InBuff)
{
	int inbytes, index;
	if((inbytes = recv(fd, InBuff, BytesToRead, 0)) == -1)
	{
		perror("server: read error -- nick"); 
		close(fd);
	}
	InBuff[inbytes] = 0;
	return inbytes;
}

void send_private(int fd, char *OutBuff)  
{
	OutBuff[strlen(OutBuff)] = '\0'; //safety is good
	if (send(fd, OutBuff, strlen(OutBuff), 0) == -1)
	{
		perror("server: send error"); 
	}
}

void broadcast_msg(int fd, char *OutBuff, fd_set *fdset, int fdmax, int listener)
{
	OutBuff[strlen(OutBuff)] = '\0';
	for(int j = 0; j <= fdmax; j++) 
	{
		// send to everyone!
		if (FD_ISSET(j, fdset)) 
		{
			// except the listener and the guy who sent it
			if (j != listener && j != fd && j!=0) 
			{
				if (send(j, OutBuff, strlen(OutBuff), 0) == -1)
				{
					perror("server: send error"); 
				}
			}
		}
	}
}


void getnick(int fd, char* nm) 
{
	char enterNick[] = "Enter a nick (Max 1023 characters) : ";
	int inbytes, len;
	char clnick[1024];
	send_private(fd, enterNick);
	if((inbytes = recv(fd, clnick, 1023, 0)) == -1)
	{
		perror("server: read error -- nick"); 
		close(fd);
	}
	if(inbytes==0)
	{
		close(fd);
		printf("Client on socket %d closed connection before entering nick\n", fd);
	}
	for(int i=0; i<inbytes-1; ++i)
	{
		*(nm+i) = clnick[i];
	}
	*(nm+inbytes-1) = '\0';
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
	fd_set readfds, masterfd;
	FD_ZERO(&readfds);
	FD_ZERO(&masterfd);
	FD_SET(listener, &masterfd);
	FD_SET(STDIN, &masterfd);
	int fdmax;

	socklen_t addrsize;
	struct sockaddr_storage their_addr;
	int inbytes,  new_fd, var;
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
					if((new_fd = accept(listener, (struct sockaddr *)&their_addr, &addrsize)) == -1) 
					{
						perror("server: accept error");
					}
					else
					{
						char c[1024];
						FD_SET(new_fd, &masterfd); // add to master set
						if (new_fd > fdmax) 
							fdmax = new_fd;
						inet_ntop(their_addr.ss_family, get_in_addr(&their_addr), incoming_IP, INET6_ADDRSTRLEN);
						printf("Server received a connection from %s on socket %d\n",incoming_IP, new_fd);
						getnick(new_fd, c);
						client_t *cli = (client_t *)malloc(sizeof(client_t));
						cli->connfd = new_fd;
						cli->uid = uid++;
						sprintf(cli->name, "%s", c);
						/* Add client to the queue */
						queue_add(cli);
						var = get_index(new_fd);
						sprintf(outmsg, "%s %s %s", "||| WELCOME",clients[var]->name, "|||\n");
						send_private(new_fd, outmsg);
						sprintf(outmsg, "%s joined the room\n",clients[var]->name);
						printf("%s", outmsg); //display on server
						broadcast_msg(new_fd, outmsg, &masterfd, fdmax, listener);
					}
				}
				else if(i == 0)
			    {
					if(fgets(outmsg, 1023, stdin)==NULL) 
						return ; 
					else
					{	
						char out[1024];
						sprintf(out, "%s %s","[Server] : ", outmsg);
						broadcast_msg(i, out, &masterfd, fdmax, listener);
					}
				}
				else //Only case left is that a client got some data
				{
					var = get_index(i);
					if(readmsg(i, 1023, inmsg))
					{
						printf("[%s] : %s", clients[var]->name, inmsg); //display on server
						sprintf(outmsg, "[%s] : %s",clients[var]->name, inmsg);
						broadcast_msg(i, outmsg, &masterfd, fdmax, listener);
					}
					else
					{
						printf("%s left the room\n", clients[var]->name); //display on server
						sprintf(outmsg, "%s %s",clients[var]->name, "left the room\n");
						broadcast_msg(i, outmsg, &masterfd, fdmax, listener);
						FD_CLR(i, &masterfd);
						close(i);
						queue_delete(i);
					}
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
	if ((err = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) //servinfo is a linked-list of info. about IPs of the host
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); 
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
