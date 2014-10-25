#include "unp.h"

int main (int argc, char ** argv){
	int sockfd, sockfd2;
	struct sockaddr_in servaddr, cliaddr;

	bzero(&servaddr, sizeof(servaddr));
	bzero(&cliaddr, sizeof(cliaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(50000);
	inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = htons(50001);
	inet_pton(AF_INET, "127.0.0.1", &cliaddr.sin_addr);
	sockfd2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
	char * testmsg = "TESTING DATAGRAM CHANNEL";

	sendto(sockfd2, testmsg, strlen(testmsg), 0, (SA*) &servaddr, sizeof(servaddr));

}

