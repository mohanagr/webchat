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
int main(int argc, char const *argv[])
{
	int sockfd, new_fd;
	fd_set readfds, masterfd;
	int fdmax;
	FD_ZERO(&readfds);
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
		fprintf(stderr, "Usage: Port\n");
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

	//add a while loop if BACKLOG > 0

	printf("Listening for incoming connections..\n");

	// their_addr stores info about incoming conn.
	sin_size = sizeof their_addr; 
	if((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) //accept  is a blocking function
	{
		perror("server: accept error");
		exit(1);
	}
	
	if(their_addr.ss_family == AF_INET) 
	{
		struct sockaddr_in *s = (struct sockaddr_in * )&their_addr;
		inet_ntop(their_addr.ss_family, &(s->sin_addr), incoming_IP, INET_ADDRSTRLEN);
	}
	else																					//inet_ntop takes size of string as argument not size of address ( as in accept() )
	{
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&their_addr;
		inet_ntop(their_addr.ss_family, &(s->sin6_addr), incoming_IP, INET6_ADDRSTRLEN);
	}
	printf("Server received a connection from %s\n",incoming_IP);

	//ask a nick
	char enterNick[] = "Enter a nick (Max 1023 characters) :";
	char clientNick[1023];
	int inbytes, i;
	enterNick[strlen(enterNick)] = '\0';
	if (send(new_fd, enterNick, strlen(enterNick), 0) == -1)
	{
		perror("server: send error"); 
		exit(5);
	}
	if((inbytes = recv(new_fd, clientNick, 1023, 0)) == -1)
	{
		perror("server: read error -- nick"); 
		exit(3);
	}
	if(inbytes==0)
	{
		close(new_fd);
		printf("Client left\n");
		exit(0);
	}
	clientNick[strlen(clientNick)-1] = '\0'; //last characters is newline which is always sent

	char outmsg[1024], inmsg[1024];

	//Multiplexing
	while(1)
	{
		FD_SET(new_fd, &readfds);
		FD_SET(STDIN, &readfds);
		fdmax = new_fd;

		if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) 
		{
 			perror("select");
 			exit(4);
 		}
		if(FD_ISSET(new_fd, &readfds))
		{
			if((inbytes = recv(new_fd, inmsg, 1023, 0)) == -1)
				perror("server: read error");
			if(inbytes==0)
			{
				close(new_fd);
				printf("%s left\n", clientNick);
				exit(0);
			}
			inmsg[inbytes] = 0; //null terminate
			printf("%s >> %s", clientNick, inmsg);
		}
		/* Add check to see if whole message has been sent in case len > 1K */
		else if(FD_ISSET(fileno(stdin), &readfds))
	    {
			if(fgets(outmsg, 1023, stdin)==NULL) //flushes buffer after firing unlike fscanf()
				return 2; 
			else
			{
				if (send(new_fd, outmsg, strlen(outmsg), 0) == -1)
					perror("server: send error");

			}
		}
				
	}
	
	close(sockfd);
 	return 0; 	
}