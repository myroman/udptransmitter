#include "unpifiplus.h"
#include <stdio.h>
#include "ifs.h"
#include "unp.h"

void handleInteraction(int sockfd, int sockOptions, InpCd* inputData);

int main()
{
	InpCd* inputData = (InpCd*)malloc(sizeof(InpCd));	
	if (parseInput(inputData) != 0) {
		return 1;
	}
	
	//determine if the server and client is one the same network
	
	char* clientIp = (char*)malloc(MAX_INPUT);
	int isServerLocal = checkIfLocalNetwork(inputData->ipAddrSrv, clientIp);
	
	if (isServerLocal == 2) {		
		clientIp = "127.0.0.1";
		inputData->ipAddrSrv = "127.0.0.1";
	}
	printf("Client IP:%s\n", clientIp);
	printf("Server Ip:%s\n", inputData->ipAddrSrv);;
	printf("Check the server is local: %d\n", isServerLocal);
	
	int sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	int sockOptions = 0;
	if (isServerLocal) {
		sockOptions = MSG_DONTROUTE;
	}
	
	//Bind ephemeral port to client IP
	struct sockaddr_in cliaddr;
	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = 0;
	struct in_addr ciaddr;
	inet_aton(clientIp, &ciaddr);
	cliaddr.sin_addr = ciaddr;
	if (bind(sockfd, (SA *)&cliaddr, sizeof(cliaddr)) == -1) {
		printf("Error when binding\n");
		exit(1);
	}
	// Getting the assigned IP address and port of the client
	socklen_t clsz = sizeof(cliaddr);
	if (getsockname(sockfd, (SA *)&cliaddr, &clsz) == -1) {
		printf("Error on getsockname\n");
		exit(1);
	}
	printf("Client's been assigned an address and port: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);	
	
	struct sockaddr_in	servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(inputData->srvPort);
	Inet_pton(AF_INET, inputData->ipAddrSrv, &servaddr.sin_addr);	
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		printf("Error when connecting\n");
		exit(1);
	}
	// Getting the server IP address and port
	socklen_t srvsz = sizeof(servaddr);
	if (getpeername(sockfd, (SA *)&servaddr, &srvsz) == -1) {
		printf("Error on getpeername\n");
		exit(1);
	}
	printf("The server address and port: %s:%d\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);
		
	handleInteraction(sockfd, sockOptions, inputData);
	return 0;
}

void handleInteraction(int sockfd, int sockOptions, InpCd* inputData) {
	const int MAX_SECS_REPLY_WAIT = 5;
	const int MAX_TIMES_TO_SEND_FILENAME = 3;
	
	int	n, i;
	char sendline[MAXLINE], recvline[MAXLINE + 1];	
	for(i = 0; i < MAX_TIMES_TO_SEND_FILENAME; ++i) {		
		send(sockfd, inputData->fileName, strlen(inputData->fileName), sockOptions);
		if (readable_timeo(sockfd, MAX_SECS_REPLY_WAIT) > 0) {
			printf("REPLY!\n");
			break;
		}
	}
	if (i == MAX_TIMES_TO_SEND_FILENAME) {
		printf("Didn't receive an ACK after %d times\n", MAX_TIMES_TO_SEND_FILENAME);
		return;
	}
	
	char* buf = (char*)malloc(MAXLINE);
	n = recv(sockfd, buf, MAXLINE, sockOptions);
	buf[n] = 0;
	printf("Received:%s", buf);
}
