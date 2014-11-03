//Soumadip Mukherjee ID: 108066531
//Roman Pavlushchenko ID: 109952457

#include "unpifiplus.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>  
#include "ifs.h"
#include "unp.h"
#include "dtghdr.h"
#include "clientCircularBuffer.h"
#include "client.h"
#include <math.h>

const int MAX_SECS_REPLY_WAIT = 5;
const int MAX_TIMES_TO_SEND_FILENAME = 12;
const int MAX_TIMES_TO_GET_CHUNK = 12;
const int MAX_TIMES_TO_READ_PORT_NUMBER = 12;
const int MAX_TIMES_TO_SEND_3RD_HANDSHAKE = 12;
	
ClientBufferNode * cHead = NULL; 	//cHead is used to setup the circular buffer
ClientBufferNode * cTail = NULL;	//cTail is used to setup the circular buffer
ClientBufferNode * start = NULL;	//start is used to symbolize where the consumer can start reading from. Only modified by consumer
ClientBufferNode * end = NULL;		//end is used to symbolize the last inorder segment received up until this point

uint32_t currentAck = 2;// = 0; //Never touch this variable. To access call getAckToSend() function.

int Finish = 0;

void rmnl(char* s);
void printBufferContents();
int sendFileNameAndGetNewServerPort(int sockfd, int sockOptions, InpCd* inputData, int* newPort, int* srvSeqN, float dropRate, struct sockaddr_in);
int sendThirdHandshake(int sockfd, int sockOptions, int lastSeqHost, float dropRate, int advWndSize);
int downloadFile(int sockfd, char* fileName, int slidingWndSize, int sockOptions, int seed, int mean, float dropRate);
void* consumeChunkRoutine (void *arg);
void* fillSlidingWndRoutine(void * arg);
int respondAckOrDrop(size_t sockfd, int sockOptions, int addFlags, float dropRate, int recvTs);
int toDropMsg(float dropRate);
pthread_mutex_t mtLock = PTHREAD_MUTEX_INITIALIZER;

void printHeading(char*s) {
	printf("***** %s *****\n", s);
}
int main()
{
	InpCd* inputData = (InpCd*)malloc(sizeof(struct inputClientData));	
	if (parseInput(inputData) != 0) {
		return 1;
	}
	//set the random seed for drand
	srand48(inputData->rndSeed);

	//determine if the server and client is one the same network	
	char* clientIp = (char*)malloc(MAX_INPUT);
	int isServerLocal = checkIfLocalNetwork(inputData->ipAddrSrv, clientIp);
	
	if (isServerLocal == 2) {		
		clientIp = "127.0.0.1";
		inputData->ipAddrSrv = "127.0.0.1";
	}

	printf("Client IP:%s, server IP:%s \n", clientIp, inputData->ipAddrSrv);
	if (isServerLocal == 0)
		printf("Client and server belong to different networks\n");
	else if (isServerLocal == 1)
		printf("Client and server belong to the same subnet. Use MSG_DONTROUTE\n");
	else if (isServerLocal == 2) 
		printf("Client and server are run on the same host. Use MSG_DONTROUTE\n");
	
	int sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	int sockOptions = 0;
	if (isServerLocal) {
		sockOptions = MSG_DONTROUTE;
	}

	printHeading("Establishing the connection");

	//Bind ephemeral port to client IP
	struct sockaddr_in cliaddr;
	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = 0;
	struct in_addr ciaddr;
	inet_aton(clientIp, &ciaddr);
	cliaddr.sin_addr = ciaddr;
	if (bind(sockfd, (SA *)&cliaddr, sizeof(cliaddr)) == -1) {
		printf("Error when binding client address\n");
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
	printf("The server-parent address and port: %s:%d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
	
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
		printf("Error when connecting to the server using fd=%d\n", sockfd);
		exit(1);
	}
	// Getting the server IP address and port
	socklen_t srvsz = sizeof(struct sockaddr_in);
	if (getpeername(sockfd, (SA *)&servaddr, &srvsz) == -1) {
		printf("Error on getpeername for server address\n");
		exit(1);
	}

	int newSrvPort, srvSeqHost;
	int res = sendFileNameAndGetNewServerPort(sockfd, sockOptions, inputData, &newSrvPort, &srvSeqHost, inputData->dtLossProb, servaddr);
	if (res == 0) {
		exit(1);
	}
	printHeading("Downloading the file");
	downloadFile(sockfd, inputData->fileName, inputData->slidWndSize, sockOptions, inputData->rndSeed, inputData->mean, inputData->dtLossProb);

	return 0;
}

// returns 0 if there's error and we have to wait for 2nd handshake
// returns 1 if we're ready for downloading the file
int sendThirdHandshake(int sockfd, int sockOptions, int lastSeqHost, float dropRate, int advWndSize) {
	int i;
	DtgHdr hdr;
		
	bzero(&hdr, sizeof(hdr));
	hdr.ack = htons(lastSeqHost + 1);	
	hdr.flags = htons(ACK_FLAG);
	printf("3rd handshake: Send advertising window of the client:%d, ACK:%d\n", advWndSize, ntohs(hdr.ack));
	
	MsgHdr msg;
	bzero(&msg, sizeof(msg));
	/*if(toDropMsg(dropRate) ==1){
		printf("3rd handshake is dropped, wait for 2nd handshake again\n");
		return 0;
	}
	*/
	fillHdr2(&hdr, &msg, NULL, 0);	
	if (sendmsg(sockfd, &msg, sockOptions) == -1) {
		printf("Error on sending 3rd handshake, wait for 2nd handshake again\n");
		return 0;
	}
	printf("3rd handshake has been just sent, waiting for the file chunks...\n");
	return 1;
}

void* consumeChunkRoutine(void *arg) {	
	ThreadArgs* targs = (ThreadArgs*)arg;
	for(;;) {
		double drandVal = (double) drand48();
		drandVal = log(drandVal);
		int sleep_time_ms= targs->mean*(-1 * drandVal ) ;
		double sleep_time_s = (double) sleep_time_ms/1000;
		sleep(sleep_time_s);

		pthread_mutex_lock(&mtLock);

		int numConsumed = consumeBuffer();

		pthread_mutex_unlock(&mtLock);

		if (Finish == 1 && numConsumed == 0){
			printf("Consumer: producer wants me to exit and I consumed 0 chunks, so I'm quitting\n");
			break;			
		}		
	}
	return NULL;
}

int toDropMsg(float dropRate){
	float drandVal = (float) drand48();
	if(drandVal <= dropRate ){
		//printf("Drop rate %f > random value %f - leave it\n", dropRate,drandVal);
		return 1;//dropAck
	}	
	else{
		//printf("Drop rate %f < random value %f - drop it!\n", dropRate,drandVal);
		return 0;//Ack back
	}
}
void* fillSlidingWndRoutine(void * arg) {
	ThreadArgs* targs = (ThreadArgs*)arg;	
	
	pthread_mutex_lock(&mtLock);
	int sockfd = targs->sockfd, 
		sockOptions = targs->sockOptions;
	pthread_mutex_unlock(&mtLock);
	
	int bufSize = getDtgBufSize(), i, n, maxRecievedSeq = 1;
	const int waitTimeoutSeconds = 5;
	for(;;) {
		for(i = 0;i < MAX_TIMES_TO_GET_CHUNK; ++i) {
			if ((n=readable_timeo(sockfd, waitTimeoutSeconds)) <= 0) {
				printf("Producer: No answer within timeout %d s\n", waitTimeoutSeconds);
				MsgHdr* rmsg = malloc(sizeof(struct msghdr));
				bzero(rmsg, sizeof(struct msghdr));
				DtgHdr* hdr = malloc(sizeof(struct dtghdr));
				bzero(hdr, sizeof(struct dtghdr));
				respondAckOrDrop(sockfd, sockOptions, 0, targs->dropRate, hdr->ts);
				continue;
			}			
			break;
		}
		if (i == MAX_TIMES_TO_GET_CHUNK) {
			printf("Producer: exceeded maximum number times to get a chunk %d\n", MAX_TIMES_TO_GET_CHUNK);

			pthread_mutex_lock(&mtLock);
			Finish = 1;
			pthread_mutex_unlock(&mtLock);
			break;
		}

		if(toDropMsg(targs->dropRate) == 1){
			printf("Producer: chunk was lost on its way to client.\n");
			continue;
		}

		//printf("Producer: A new chunk has come! Reading it... \n");

		char* chunkBuf = malloc(bufSize);
		MsgHdr* rmsg = malloc(sizeof(struct msghdr));
		bzero(rmsg, sizeof(struct msghdr));
		DtgHdr* hdr = malloc(sizeof(struct dtghdr));
		bzero(hdr, sizeof(struct dtghdr));
		
		fillHdr2(hdr, rmsg, chunkBuf, bufSize);
		if ((n = recvmsg(sockfd, rmsg, 0)) == -1) {
			printf("Producer: Error when reading the message: %s. Quitting.\n", strerror(errno));
			pthread_mutex_lock(&mtLock);
			Finish = 1;
			pthread_mutex_unlock(&mtLock);
			break;
		}
		
		int sentFlags = ntohs(hdr->flags);		
		
		if (sentFlags != FIN_FLAG) {		
			pthread_mutex_lock(&mtLock);
			//char* s = extractBuffFromHdr(*rmsg);
			//printf("P: gonna add to buffer seq=%d, expecting seq=%d\n", ntohs(hdr->seq), getAckToSend());
			//printf("P:buf %s", s);
			if(availableWindowSize() == 0){
				printf("Producer: No space left in the buffer. Discarding the message SEQ=%d\n", ntohs(hdr->seq));
				//pthread_mutex_unlock(&mtLock);
				//continue;
			}
			else if(ntohs(hdr->seq) < maxRecievedSeq){
				printf("Producer: Duplicate message SEQ=%d! Already processed and added to the buffer\n", ntohs(hdr->seq));
				addDataPayload(ntohs(hdr->seq), chunkBuf,rmsg);
			}
			else{
				printf("Producer: I've read the chunk: SEQ:%d, going to add it to the buffer\n", ntohs(hdr->seq));
			
				maxRecievedSeq = ntohs(hdr->seq);
				int n = addDataPayload(ntohs(hdr->seq), chunkBuf,rmsg);
			}
			
			//printBufferContents();
			//printf("P: added, n = %d\n", n);									
			
			pthread_mutex_unlock(&mtLock);			
			respondAckOrDrop(sockfd, sockOptions, 0, targs->dropRate, hdr->ts);
		}
		else {
			printf("Producer:I got a FIN\n");
			pthread_mutex_lock(&mtLock);
			Finish = 1;

			// Send a final ACK which should terminate the connection
			respondAckOrDrop(sockfd, sockOptions, FIN_FLAG, targs->dropRate, hdr->ts);
			pthread_mutex_unlock(&mtLock);

			printf("Producer: Quitting the receive loop\n");
			break;
		}
	}
	return NULL;
}

// Used to send ACKs to server or FIN+ACK
int respondAckOrDrop(size_t sockfd, int sockOptions, int addFlags, float dropRate, int recvTs) {
	DtgHdr hdr;
	bzero(&hdr, sizeof(hdr));
	int ack = getAckToSend();

	hdr.ack = htons(ack);
	hdr.flags = htons(ACK_FLAG | addFlags);
	hdr.advWnd = htons(availableWindowSize());
	hdr.ts = recvTs; //network	
	MsgHdr msg;
	bzero(&msg, sizeof(msg));	

	if(toDropMsg(dropRate) == 1 ){
		printf("Producer: ACK %d was lost on its way to the server.\n", ack);
		return 2;
	}
	
	fillHdr2(&hdr, &msg, NULL, 0);
	if (sendmsg(sockfd, &msg, sockOptions) == -1) {
		printf("Producer: Error on sending ACK %d\n", ack);
		return 0;
	}
	if (addFlags == FIN_FLAG)
		printf("Producer: responded with FIN+ACK %d\n", ack);
	else
		printf("Producer: responded with ACK %d, adv.windows size %d\n", ack, ntohs(hdr.advWnd));
	return 1;
}

int downloadFile(int sockfd, char* fileName, int slidingWndSize, int sockOptions, int seed, int mean, float dropRate) {
	ThreadArgs* targs = malloc(sizeof(struct threadArgs));
	targs->sockfd = sockfd;
	targs->fileName = fileName;
	targs->sockOptions = sockOptions;
	targs->seed = seed;
	targs->mean = mean;
	targs->dropRate = dropRate;
	
	int res = allocateCircularBuffer(slidingWndSize);
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

int sendFileNameAndGetNewServerPort(int sockfd, int sockOptions, InpCd* inputData, int* newPort, int* srvSeqNumber, float dropRate, struct sockaddr_in servaddr) {	
	rmnl(inputData->fileName);

	int	n, i;
	for(i = 0; i < MAX_TIMES_TO_SEND_FILENAME; ++i) {	
		DtgHdr sendHdr;
		bzero(&sendHdr, sizeof(sendHdr));
		sendHdr.seq = htons(1);
		sendHdr.advWnd = htons(inputData-> slidWndSize);
		MsgHdr smsg;
		bzero(&smsg, sizeof(smsg));
		
		printf("1st handshake: send file name: %s (SEQ=%d), advertising window: %d\n", inputData->fileName, ntohs(sendHdr.seq), inputData->slidWndSize);
		fillHdr2(&sendHdr, &smsg, inputData->fileName, getDtgBufSize());
		if(toDropMsg(dropRate) == 1){//simulate the dropping of first handshake
			printf("Message with SEQ %d has been lost on the way to the server, try sending file name again\n", ntohs(sendHdr.seq));
			continue;
		}
		if (sendmsg(sockfd, &smsg, 0) == -1) {
			printf("Error on sending the file name\n");
			return 0;
		}
		printf("File name's been just sent, waiting for new server port...\n");
		int j = 0, portIsSet = 0;
		for(j = 0; j < MAX_TIMES_TO_READ_PORT_NUMBER ; j++ ){
			int prevSockfd = sockfd;
			// if 2nd handshake within the timeout, try 1st handshake once more
			if (readable_timeo(prevSockfd, MAX_SECS_REPLY_WAIT) > 0) {
				// Read server ephemeral port number
				MsgHdr rmsg;
				DtgHdr secondHsHdr;
				bzero(&secondHsHdr, sizeof(secondHsHdr));
				char* buf = (char*)malloc(MAXLINE);
				fillHdr2(&secondHsHdr, &rmsg, buf, MAXLINE);

				if ((n = recvmsg(prevSockfd, &rmsg, 0)) == -1) {
					printf("Error on receiving the port number\n");
					return 0;
				}
				if(toDropMsg(dropRate) == 1){//this will simulate dthe dropping of second handshake
					printf("The port number has been lost when coming to the client, try sending file name again\n");
					continue;
				}
				if (portIsSet == 0) {
					*srvSeqNumber = ntohs(secondHsHdr.seq);
					int tmp=0;
					if (sscanf(buf, "%d", &tmp) == 0) {
						printf("Server should have been sent port number but sent a string: %s\n", buf);
						return 1;
					}

					*newPort = tmp;

					free(buf);
					servaddr.sin_port = *newPort;

					if (connect(prevSockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
						printf("Error when reconnecting to new server port\n");
						exit(1);
					}				
					printf("Reconnected to a new server address %s:%d\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);

					portIsSet = 1;
				}
				printf("2nd handshake: client has received port:%d (SEQ:%d), going to send out an ACK\n", *newPort, *srvSeqNumber);				
				
				if (sendThirdHandshake(prevSockfd, sockOptions, *srvSeqNumber, dropRate, inputData->slidWndSize) == 0) {
					continue;
				}
				return 1;			
			}
			else {
				printf("Timeout of reading 2nd handshake expired\n");
			}
		}
		printf("Client didn't receive a valid 2nd handshake after %d times, give up.\n", MAX_TIMES_TO_SEND_FILENAME);
		return 0;
	}
	printf("Client didn't receive a valid 2nd handshake after %d times, give up.\n", MAX_TIMES_TO_SEND_FILENAME);
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
	//printf("P:expecting segment:%d, s=%u, start->occupied=%d\n", getAckToSend(), s, start->occupied);
	//printBufferContents();

	if(start->occupied == 0 && s == getAckToSend()){
		start->occupied = 1;
		start->seqNum = s;
		start->dataPayload = dp;
		start->cptr =c;
	}
	else if(start->occupied ==0 && s != getAckToSend()){
		//printf("P: NOT EXPECTING THIS PACKET.\n");
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

int consumeBuffer() {
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

			printf("*** Pack Sequence Number: %d *** \n Contents: %s\n******\n", start->seqNum, start->cptr);
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
		MsgHdr *mptr = start->dataPayload;
		char *toWrite = extractBuffFromHdr(*mptr);
		printf("*** Pack Sequence Number: %d *** \n Contents: %s\n******\n", start->seqNum, start->cptr);
		//Write out the data to the File
		//MsgHdr *mptr = start->dataPayload;
		//char *toWrite = extractBuffFromHdr(*mptr);
		//fwrite(toWrite, sizeof(toWrite), 1,  fPointer);

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

void rmnl(char* s) {
	int ln = strlen(s) - 1;
	if (s[ln] == '\n')
	    s[ln] = '\0';
}