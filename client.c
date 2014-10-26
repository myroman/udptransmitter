#include "unpifiplus.h"
#include <stdio.h>
#include "ifs.h"
#include "unp.h"
#include "dtghdr.h"

int sendFileNameAndGetNewServerPort(int sockfd, int sockOptions, InpCd* inputData, int* newPort, int* srvSeqN);
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
	printf("server well-known port: %d\n", inputData->srvPort);
	Inet_pton(AF_INET, inputData->ipAddrSrv, &servaddr.sin_addr);	
	printf("The server address and port: %s:%d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
	
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		printf("Error when connecting\n");
		exit(1);
	}
	// Getting the server IP address and port
	socklen_t srvsz = sizeof(struct sockaddr_in);
	if (getpeername(sockfd, (SA *)&servaddr, &srvsz) == -1) {
		printf("Error on getpeername\n");
		exit(1);
	}
	printf("The server address and port: %s:%d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
		
	int newSrvPort, srvReplySeq;
	int res = sendFileNameAndGetNewServerPort(sockfd, sockOptions, inputData, &newSrvPort, &srvReplySeq);
	if (res == 0) {
		exit(1);
	}

	printf("New srv port: %d\n", newSrvPort);
	servaddr.sin_port = newSrvPort;

	//close(sockfd);
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		printf("Error when reconnecting to new server port\n");
		exit(1);
	}
	printf("%s:%d\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);

	DtgHdr sendHdr;
	bzero(&sendHdr, sizeof(DtgHdr));
	sendHdr.ack = srvReplySeq + 1;
	printf("ack %d\n", sendHdr.ack);

	MsgHdr smsg;
	bzero(&smsg, sizeof(MsgHdr));
	
	fillHdr2(&sendHdr, &smsg, NULL, 0);
	if (sendmsg(sockfd, &smsg, sockOptions) == -1) {
		printf("Error on sendmsg\n");
		return 1;
	}
	return 0;
}

int sendFileNameAndGetNewServerPort(int sockfd, int sockOptions, InpCd* inputData, int* newPort, int* srvSeqN) {
	const int MAX_SECS_REPLY_WAIT = 5;
	const int MAX_TIMES_TO_SEND_FILENAME = 3;
	
	int	n, i;
	char sendline[MAXLINE], recvline[MAXLINE + 1];	
	for(i = 0; i < MAX_TIMES_TO_SEND_FILENAME; ++i) {	
		DtgHdr sendHdr;
		bzero(&sendHdr, sizeof(DtgHdr));
		sendHdr.seq = 1;

		MsgHdr smsg;
		bzero(&smsg, sizeof(MsgHdr));
		printf(inputData->fileName);
		fillHdr2(&sendHdr, &smsg, inputData->fileName, 500);

		if (sendmsg(sockfd, &smsg, 0) == -1) {
			printf("Error on sendmsg\n");
			return 0;
		}
		char* fn = smsg.msg_iov[1].iov_base;
		printf("%d\n", smsg.msg_iov[1].iov_len);
		printf("Sent the file name %s...\n", fn);

		if (readable_timeo(sockfd, MAX_SECS_REPLY_WAIT) > 0) {
			printf("REPLY!\n");
			break;
		}
	}
	if (i == MAX_TIMES_TO_SEND_FILENAME) {
		printf("Didn't receive an ACK after %d times\n", MAX_TIMES_TO_SEND_FILENAME);
		return 0;
	}
	
	MsgHdr rmsg;
	DtgHdr rHdr;
	bzero(&rHdr, sizeof(DtgHdr));
	char* buf = (char*)malloc(MAXLINE);
	fillHdr2(&rHdr, &rmsg, buf, MAXLINE);
	if ((n = recvmsg(sockfd, &rmsg, 0)) == -1) {
		printf("Error on recvmsg\n");
		return 0;
	}
	
	DtgHdr* replyHdr = (DtgHdr*)rmsg.msg_iov[0].iov_base;
	*srvSeqN = replyHdr->seq;
	printf("seq:%u\n", *srvSeqN);

	printf("n: %d\n", n);
	printf("%s", buf);
	*newPort = atoi(buf); //TODO: gotta check it
	printf("New server ephemeral port: %d\n", *newPort);
	return 1;
}