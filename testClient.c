#include "unp.h"

int main (int argc, char ** argv){
	int sockfd;
	struct sockaddr_in servaddr;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(50000);
	inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	char * testmsg = "TESTING DATAGRAM CHANNEL";

	sendto(sockfd, testmsg, strlen(testmsg), 0, (SA*) &servaddr, sizeof(servaddr));

}