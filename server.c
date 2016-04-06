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

#define BACKLOG 0 //queue of pending requests
#define STDIN 0

void readmsg(int fd, int BytesToRead, char *InBuff)
{
	int inbytes;
	if((inbytes = recv(fd, InBuff, BytesToRead, 0)) == -1)
	{
		perror("server: read error -- nick"); 
		exit(1);
	}
	if(inbytes==0)
	{
		close(fd);
		printf("Client left\n");
		exit(0);
	}
	InBuff[inbytes] = 0;
	return;
}

void send_msg(int fd, char *OutBuff)  //sendmsg is already a function in socket.h
{
	OutBuff[strlen(OutBuff)] = '\0'; //safety is good
	if (send(fd, OutBuff, strlen(OutBuff), 0) == -1)
	{
		perror("server: send error"); 
		exit(1);
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

void multiplexer(int fd, char nick[])
{
	fd_set readfds, masterfd;
	int fdmax = fd;
	FD_ZERO(&readfds);
	char outmsg[1024], inmsg[1024];
	while(1)
	{
		FD_SET(fd, &readfds);
		FD_SET(STDIN, &readfds);
		int inbytes;

		if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) 
		{
 			perror("select");
 			exit(4);
 		}
		if(FD_ISSET(fd, &readfds))
		{
			readmsg(fd, 1023, inmsg);
			printf("%s >> %s", nick, inmsg);
		}
		/* Add check to see if whole message has been sent in case len > 1K */
		else if(FD_ISSET(fileno(stdin), &readfds))
	    {
			if(fgets(outmsg, 1023, stdin)==NULL) //flushes buffer after firing unlike fscanf()
				return ; 
			else
			{
				send_msg(fd, outmsg);
			}
		}
				
	}
}
int main(int argc, char const *argv[])
{
	int sockfd, new_fd;

	char incoming_IP[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *var;
	struct sockaddr_storage their_addr; // since we do not know whether incoming conn. is INET or INET6 (Alternatively use sockaddr_in / sockaddr_in6)
	socklen_t sin_size;
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

	// their_addr stores info about incoming conn.
	sin_size = sizeof their_addr; 
	if((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) //accept  is a blocking function
	{
		perror("server: accept error");
		exit(1);
	}
	
	/*if(their_addr.ss_family == AF_INET) 
	{
		struct sockaddr_in *s = (struct sockaddr_in * )&their_addr;
		inet_ntop(their_addr.ss_family, &(s->sin_addr), incoming_IP, INET_ADDRSTRLEN);
	}
	else																					//inet_ntop takes size of string as argument not size of address ( as in accept() )
	{
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&their_addr;
		inet_ntop(their_addr.ss_family, &(s->sin6_addr), incoming_IP, INET6_ADDRSTRLEN);
	}*/

	inet_ntop(their_addr.ss_family, get_in_addr(&their_addr), incoming_IP, INET6_ADDRSTRLEN);
	
	printf("Server received a connection from %s\n",incoming_IP);

	//ask a nick
	char client_nick[1024];
	getnick(new_fd, client_nick);

	//Multiplexing
	multiplexer(new_fd, client_nick);
	close(sockfd);
 	return 0; 	
}