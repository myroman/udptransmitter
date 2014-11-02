#include "unpifiplus.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>  
#include "ifs.h"
#include "unp.h"
#include "dtghdr.h"
#include "utils.h"
#include "clientCircularBuffer.h"
#include "client.h"
#include <math.h>

const int MAX_SECS_REPLY_WAIT = 5;
const int MAX_TIMES_TO_SEND_FILENAME = 3;
	
ClientBufferNode * cHead = NULL; 	//cHead is used to setup the circular buffer
ClientBufferNode * cTail = NULL;	//cTail is used to setup the circular buffer
ClientBufferNode * start = NULL;	//start is used to symbolize where the consumer can start reading from. Only modified by consumer
ClientBufferNode * end = NULL;		//end is used to symbolize the last inorder segment received up until this point

uint32_t currentAck = 2;// = 0; //Never touch this variable. To access call getAckToSend() function.

int Finish = 0;

void printBufferContents();
int sendFileNameAndGetNewServerPort(int sockfd, int sockOptions, InpCd* inputData, int* newPort, int* srvSeqN);
int sendThirdHandshake(int sockfd, int sockOptions, int lastSeqHost);
int downloadFile(int sockfd, char* fileName, int slidingWndSize, int sockOptions, int seed, int mean, float dropRate);
void* consumeChunkRoutine (void *arg);
void* fillSlidingWndRoutine(void * arg);
int respondAckOrDrop(size_t sockfd, int sockOptions, int addFlags, float dropRate, int recvTs);

pthread_mutex_t mtLock = PTHREAD_MUTEX_INITIALIZER;

int main()
{
	InpCd* inputData = (InpCd*)malloc(sizeof(struct inputClientData));	
	if (parseInput(inputData) != 0) {
		return 1;
	}
	printf("DropRate: %f\n",inputData->dtLossProb);
	//set the random seed for drand
	srand48(inputData->rndSeed);

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

	int newSrvPort, srvSeqHost;
	// 1st and 2nd handshakes
	int res = sendFileNameAndGetNewServerPort(sockfd, sockOptions, inputData, &newSrvPort, &srvSeqHost);
	if (res == 0) {
		exit(1);
	}

	servaddr.sin_port = newSrvPort;

	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		printf("Error when reconnecting to new server port\n");
		exit(1);
	}
	printf("%s:%d\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);

	DtgHdr sendHdr;
	bzero(&sendHdr, sizeof(sendHdr));
	//sendHdr.ack = srvReplySeq + 1;	
	printf("I got ack %d\n", sendHdr.ack);
	sendHdr.ack = htons(sendHdr.ack);
	sendHdr.flags = htons(ACK_FLAG);
	sendHdr.advWnd = htons(inputData->slidWndSize);
	MsgHdr smsg;
	bzero(&smsg, sizeof(smsg));
	printf("Reconnected to new server address %s:%d\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);
	//TODO simulate dropping for third Handshake
	if (sendThirdHandshake(sockfd, sockOptions, srvSeqHost) == 0) {
		return 1;
	}
	downloadFile(sockfd, inputData->fileName, inputData->slidWndSize, sockOptions, inputData->rndSeed, inputData->mean, inputData->dtLossProb);

	return 0;
}

int sendThirdHandshake(int sockfd, int sockOptions, int lastSeqHost) {
	DtgHdr hdr;
	bzero(&hdr, sizeof(hdr));
	hdr.ack = htons(lastSeqHost + 1);	
	hdr.flags = htons(ACK_FLAG);
	printf("Gonna send 3rd hs, ACK:%d,flags:%d\n", ntohs(hdr.ack), ntohs(hdr.flags));
	
	MsgHdr msg;
	bzero(&msg, sizeof(msg));
	
	fillHdr2(&hdr, &msg, NULL, 0);	
	if (sendmsg(sockfd, &msg, sockOptions) == -1) {
		printf("Error on sending 3rd handshake\n");
		return 0;
	}
	printf("I've sent 3rd handshake\n");
	return 1;
}

void* consumeChunkRoutine(void *arg) {	
	ThreadArgs* targs = (ThreadArgs*)arg;
	printf("seed: %d, mean: %d\n", targs->seed, targs->mean);
	pthread_mutex_lock(&mtLock);
	char* tmpFn = targs->fileName;
	pthread_mutex_unlock(&mtLock);
	char* fileName = malloc(strlen(tmpFn) + 4);
	strcpy(fileName, "cli_");
	strcat(fileName, tmpFn);
	FILE* dlFile = fopen(fileName, "w+");
	//int mean = 1234;
	for(;;) {
		double drandVal = (double) drand48();
		drandVal = log(drandVal);
		int sleep_time_ms= targs->mean*(-1 * drandVal ) ;
		double sleep_time_s = (double) sleep_time_ms/1000;
		sleep(sleep_time_s);

		//sleep(rand()%3);//5);//TODO: random

		pthread_mutex_lock(&mtLock);

		int numConsumed = consumeBuffer(dlFile);
		printf("C:consumed %d datagrams\n", numConsumed);

		pthread_mutex_unlock(&mtLock);

		if (Finish == 1 && numConsumed == 0){//targs->fin ==1 && numConsumed == 0) {
	
			printf("C:found out EOF, so I save the file\n");
			//pthread_mutex_unlock(&mtLock);
			break;			
		}
		
	}
	fclose(dlFile);
	free(fileName);
	return NULL;
}

void sendFinToConsumer(ThreadArgs* targs){
	pthread_mutex_lock(&mtLock);
	targs->fin = 1;
	pthread_mutex_unlock(&mtLock);
}
int toDropMsg(float dropRate){
	float drandVal = (float) drand48();
	printf("toDropMsg:: dropRate: %f, randValue%f \n", dropRate,drandVal);
	if(drandVal <= dropRate ){
		return 1;//dropAck
	}	
	else{
		return 0;//Ack back
	}
}
void* fillSlidingWndRoutine(void * arg) {
	ThreadArgs* targs = (ThreadArgs*)arg;	
	pthread_mutex_lock(&mtLock);
	int sockfd = targs->sockfd, sockOptions = targs->sockOptions;
	pthread_mutex_unlock(&mtLock);
	int maxRecievedSeq = 1;
	int bufSize = getDtgBufSize();
	//char* chunkBuf = malloc(bufSize);
	int i, n;
	
	for(;;) {
		for(i = 0;i < MAX_TIMES_TO_SEND_FILENAME; ++i) {
			printf("P: wait for read from socket:%d\n", sockfd);			
			if ((n=readable_timeo(sockfd, 5)) <= 0) {
				continue;
			}
			break;
		}
		if (i == MAX_TIMES_TO_SEND_FILENAME) {
			sendFinToConsumer(targs);
			break;
		}
		char* chunkBuf = malloc(bufSize);
		MsgHdr* rmsg = malloc(sizeof(struct msghdr));
		bzero(rmsg, sizeof(struct msghdr));
		DtgHdr* hdr = malloc(sizeof(struct dtghdr));
		bzero(hdr, sizeof(struct dtghdr));
		
		fillHdr2(hdr, rmsg, chunkBuf, bufSize);
		if ((n = recvmsg(sockfd, rmsg, 0)) == -1) {
			printf("Error: %s\n", strerror(errno));
			printf("P: Error on recvmsg from socket %d\n", sockfd);
			
			sendFinToConsumer(targs);
			break;
		}
		
		//TODO: simulate dropping of receiving

		int sentFlags = ntohs(hdr->flags);		
		printf("P: received seq:%d, flags: %d\n", ntohs(hdr->seq), sentFlags);
		if(toDropMsg(targs->dropRate) == 1){
			printf("Message dropped.\n");
			continue;
		}	
		printf("P: I received\n");
		if (sentFlags != FIN_FLAG) {		
			pthread_mutex_lock(&mtLock);
			//char* s = extractBuffFromHdr(*rmsg);
			printf("P: gonna add to buffer seq=%d, expecting seq=%d\n", ntohs(hdr->seq), getAckToSend());
			//printf("P:buf %s", s);
			if(availableWindowSize() == 0){
				printf("NO SPACE LEFT IN BUFFER. DISGARDING MESSAGE\n");
				//pthread_mutex_unlock(&mtLock);
				//continue;
			}
			else if(ntohs(hdr->seq) < maxRecievedSeq){
				printf("DUPLICATE MESSAGE! ALREADY PROCESSED AND ADDED TO BUFFER\n");
				addDataPayload(ntohs(hdr->seq), chunkBuf,rmsg);
				//printBufferContents();
			}
			else{
				
				maxRecievedSeq = ntohs(hdr->seq);
				int n = addDataPayload(ntohs(hdr->seq), chunkBuf,rmsg);
			}
			
			//printBufferContents();
			//printf("P: added, n = %d\n", n);									
			
			pthread_mutex_unlock(&mtLock);			
			respondAckOrDrop(sockfd, sockOptions, 0, targs->dropRate, hdr->ts);
		}
		else {
			printf("P:Got a FIN\n");
			pthread_mutex_lock(&mtLock);
			//printf("P: after acquiring lock\n");
			Finish = 1;
			//sendFinToConsumer(targs);

			// Send a final ACK which should terminate the connection
			respondAckOrDrop(sockfd, sockOptions, FIN_FLAG, targs->dropRate, hdr->ts);
			pthread_mutex_unlock(&mtLock);
			printf("wanna exit\n");
			break;
		}
	}
	//free(chunkBuf);
	return NULL;
}

// Used to send ACKs to server or FIN+ACK
int respondAckOrDrop(size_t sockfd, int sockOptions, int addFlags, float dropRate, int recvTs) {
	DtgHdr hdr;
	bzero(&hdr, sizeof(hdr));
	hdr.ack = htons(getAckToSend());
	hdr.flags = htons(ACK_FLAG | addFlags);
	printf("Adv window size: %d\n", availableWindowSize());
	hdr.advWnd = htons(availableWindowSize());
	hdr.ts = recvTs; //network
	printf("P: respond with ACK:%d, flags: %d\n", ntohs(hdr.ack), ntohs(hdr.flags));
	
	MsgHdr msg;
	bzero(&msg, sizeof(msg));	
	
	//TODO: sim.dropping of ACKing
	if(toDropMsg(dropRate) == 1 ){
		//Ack dropped
		printf("ACK dropped.\n");
		return 2;
	}
	printf("P:Gonna send msg\n");
	fillHdr2(&hdr, &msg, NULL, 0);
	if (sendmsg(sockfd, &msg, sockOptions) == -1) {
		printf("P:Error on send ack\n");
		return 0;
	}

	return 1;
}

int downloadFile(int sockfd, char* fileName, int slidingWndSize, int sockOptions, int seed, int mean, float dropRate) {
	int res;
	
	ThreadArgs* targs = malloc(sizeof(struct threadArgs));
	targs->sockfd = sockfd;
	targs->fileName = fileName;
	targs->sockOptions = sockOptions;
	targs->seed = seed;
	targs->mean = mean;
	targs->dropRate = dropRate;
	
	res = allocateCircularBuffer(slidingWndSize);
	if (res == -1) {
		printf("Circular buffer failed to allocate\n");
		return 0;
	}

	pthread_t consumerThr;	
	pthread_create(&consumerThr, NULL, (void *)&consumeChunkRoutine, (void*)targs);
	
	pthread_t producerThr;	
	pthread_create(&producerThr, NULL, (void *)&fillSlidingWndRoutine, (void*)targs);

	pthread_join(consumerThr, NULL);
	pthread_join(producerThr, NULL);

	free(targs);
	return 1;
}

int sendFileNameAndGetNewServerPort(int sockfd, int sockOptions, InpCd* inputData, int* newPort, int* srvSeqNumber) {	
	rmnl(inputData->fileName);

	int	n, i;
	for(i = 0; i < MAX_TIMES_TO_SEND_FILENAME; ++i) {	
		DtgHdr sendHdr;
		bzero(&sendHdr, sizeof(sendHdr));
		sendHdr.seq = htons(1);
		sendHdr.advWnd = htons(inputData-> slidWndSize);
		MsgHdr smsg;
		bzero(&smsg, sizeof(smsg));
		
		printf("Gonna send filename: %s, seq=%d\n", inputData->fileName, ntohs(sendHdr.seq));
		fillHdr2(&sendHdr, &smsg, inputData->fileName, getDtgBufSize());

		if (sendmsg(sockfd, &smsg, 0) == -1) {
			printf("Error on sendmsg\n");
			return 0;
		}
		printf("Sent it!\n");

		// if 2nd handshake within the timeout, try 1st handshake once more
		if (readable_timeo(sockfd, MAX_SECS_REPLY_WAIT) > 0) {
			// Read server ephemeral port number
			MsgHdr rmsg;
			DtgHdr secondHsHdr;
			bzero(&secondHsHdr, sizeof(secondHsHdr));
			char* buf = (char*)malloc(MAXLINE);
			fillHdr2(&secondHsHdr, &rmsg, buf, MAXLINE);
			if ((n = recvmsg(sockfd, &rmsg, 0)) == -1) {
				printf("Error on recvmsg\n");
				return 0;
			}
			*srvSeqNumber = ntohs(secondHsHdr.seq);
			*newPort = atoi(buf); //TODO: gotta check it
			free(buf);
			printf("Received 2nd handshake, port:%d, seq:%d, flags:%d\n", *newPort, *srvSeqNumber, ntohs(secondHsHdr.flags));
			return 1;			
		}
	}
	printf("Didn't receive a 2nd handshake after %d times\n", MAX_TIMES_TO_SEND_FILENAME);
	return 0;
}

int allocateCircularBuffer(int numToAllocate) {
	
	if(numToAllocate < 1) {
		printf("Invalid number of nodes to allocate...\n");
		return -1;
	}
	ClientBufferNode * cptr = malloc(sizeof(ClientBufferNode) * numToAllocate);
	bzero(cptr, sizeof(ClientBufferNode) * numToAllocate);
	if(cptr == NULL){
		printf("Out of Memory. Exiting...");
		return -1;
	}	
	int i;
	if(numToAllocate > 2){
		for(i = 0; i < numToAllocate ; i++){
			if(i == 0){
				cHead = &cptr[i];
				cptr[i].left = &cptr[numToAllocate-1];
				cptr[i].right = &cptr[i+1];
			}
			else if(i ==(numToAllocate -1)){
				cTail = &cptr[i];
				cptr[i].right = &cptr[0];
				cptr[i].left = &cptr[i-1];
			}
			else{
				cptr[i].right = &cptr[i+1];
				cptr[i].left = &cptr[i-1];
			}
		}
	}
	else if(numToAllocate == 1){
		cHead = &cptr[0];
		cTail = &cptr[0];
		cptr[0].right = &cptr[0];
		cptr[0].left = &cptr[0];
	}
	else {
		cHead = &cptr[0];
		cTail = &cptr[1];
		cptr[0].right = &cptr[1];
		cptr[0].left = &cptr[1];
		cptr[1].right = &cptr[0];
		cptr[1].left = &cptr[0];
	}
	start = cHead;
	end = cHead;
	return 1;
}

int availableWindowSize(){
	ClientBufferNode* ptr = cHead;
	int count = 0;
	do{
		if (ptr->occupied == 0)
			++count;
		ptr = ptr->right;
	}while(ptr!= cHead);
	return count;
}

int getWindowSize(){
	int count = 0;
	ClientBufferNode *ptr = cHead;
	do{
		count++;
		ptr = ptr->right;
	} while(ptr!= cHead);
	return count;
}

uint32_t getAckToSend(){
	return currentAck;
}

void updateInorderEnd(){
	if(end->occupied == 0){
		return;
	}

	ClientBufferNode * ptr = end;
	do{
		if(ptr->right->occupied == 0){
			break;
		}
		ptr=ptr->right;// this will advance ptr
	}while(ptr != end); //go around this loop a max of once
	end = ptr;//set end to ptr;
	currentAck = (ptr->seqNum) + 1;//sets the currentAck field 
}

int addDataPayload(uint32_t s, char * c,MsgHdr* dp) {
	if(availableWindowSize() == 0){
		return -1;
	}
	//printf("AddDataPayload: %s\n\n", c);
	printf("P:expecting segment:%d, s=%u, start->occupied=%d\n", getAckToSend(), s, start->occupied);
	//printBufferContents();

	if(start->occupied == 0 && s == getAckToSend()){
		start->occupied = 1;
		start->seqNum = s;
		start->dataPayload = dp;
		start->cptr =c;
	}
	else if(start->occupied ==0 && s != getAckToSend()){
		printf("P: NOT EXPECTING THIS PACKET.\n");
	}
	else{
		int difference, i;
		difference = s - start->seqNum;//this will get how many nodes to go forward 
		ClientBufferNode *ptr = start;
		for(i = 0; i < difference; i++){
			ptr = ptr->right;
		}

		if(ptr->occupied == 1){//if its occupied just return with 0
			return 0; 
		}
		ptr->occupied = 1;
		ptr->seqNum = s;
		ptr->dataPayload = dp;
		ptr->cptr =c;

	}
	//printBufferContents();
	updateInorderEnd();//this will update the pointer to the end
	return 1;//successfully added data
}

int consumeBuffer(FILE * fPointer) {
	int count = 0;
	if(start->occupied == 0){
		return count;//nothing to read yet
	}
	
	if(getWindowSize() >= 2){
		//printBufferContents();
		do {
			count++;
			//Write out the data to the File
			MsgHdr *mptr = start->dataPayload;
			char *toWrite = extractBuffFromHdr(*mptr);
			//int seqNum = data[0].iov_base = hdr;
			printf("Pack Sequence Number: %d\n Contents: %s\n", start->seqNum, start->cptr);
			//fwrite(toWrite, sizeof(toWrite), 1,  fPointer);

			// ********

			start->occupied = 0;

			free(start->dataPayload);

			start->dataPayload=NULL;
			start = start->right;
		} 
		while(start != end->right);
		end = start;
	}
	else{
		count++;
		//Write out the data to the File
		MsgHdr *mptr = start->dataPayload;
		char *toWrite = extractBuffFromHdr(*mptr);
		fwrite(toWrite, sizeof(toWrite), 1,  fPointer);

		start->occupied = 0;
		start->dataPayload=NULL;
	}
	return count;
}

void printBufferContents(){
	ClientBufferNode * ptr = cHead;
	int count =0;
	printf("\n***** DUMP Buffer Contents: *****\n");
	do{
	printf("Node index: %d \n", count);
	printf("\tOccupied: %d, SequenceNumber: %d, PayloadPtr %u\n\n", ptr->occupied, ptr->seqNum, ptr->dataPayload);
	ptr = ptr-> right;
	count++;
	}while(ptr!= cHead);
	printf("\n**********************************\n");
}