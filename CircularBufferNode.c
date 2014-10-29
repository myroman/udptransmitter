#include "dtghdr.h"
#include "unp.h"

typedef struct ServerBufferNode ServerBufferNode;//typeDef for the Clinet Info object
struct ServerBufferNode{
    int occupied;//Process ID used to remove from the DS when server is done serving client
    uint32_t  seq;
    uint32_t ackCount;
    uint32_t ts;
    MsgHdr * dataPayload;
    ServerBufferNode *right;
    ServerBufferNode *left;
};

ServerBufferNode * cHead = NULL;
ServerBufferNode * cTail = NULL;
ServerBufferNode * start = NULL;
ServerBufferNode * end = NULL;
/*
* This function will allocate the number of nodes in the Circular buffer
* @ numToAllocate is the number of elements to allocate for
* @ return -1 error
* 			1 success
*/
int allocateCircularBuffer(int numToAllocate){
	if(numToAllocate < 1){
		printf("Invalid number of nodes to allocate...\n");
		return -1;
	}
	ServerBufferNode * cptr = malloc(sizeof(ServerBufferNode) * numToAllocate);
	if(cptr == NULL){
		printf("Out of Memory. Exiting...");
		return -1;
	}	
	int i;
	if(numToAllocate > 2){
		printf("Here\n");
		for(i = 0; i < numToAllocate ; i++){
			if(i == 0){
				printf("Here HEAD\n");
				cHead = &cptr[i];
				cptr[i].left = &cptr[numToAllocate-1];
				cptr[i].right = &cptr[i+1];
			}
			else if(i ==(numToAllocate -1)){
				printf("Here TAIL\n");
				cTail = &cptr[i];
				cptr[i].right = &cptr[0];
				cptr[i].left = &cptr[i-1];
			}
			else{
				printf("Here MIDDLE\n");
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
/*
* This will return the number of nodes that not currently occupied
* @ return - the number of nodes that are free
*/
int availableWindowSize(){
	ServerBufferNode* ptr = cHead;
	int count = 0;
	do{
		if (ptr->occupied == 0)
			++count;
		ptr = ptr->right;
	}while(ptr!= cHead);
	return count;
}

/*
* This function will find a node in the circular buffer that has the interger specified by the sequence number
* @ seqNum that identifies the sequence number taht needs to be found
* @ return the pointer to the node containing the sequence number or a NULL pointer symbolizing that it was not found
*/
ServerBufferNode * findSeqNode(uint32_t seqNum){
	ServerBufferNode * ptr = start;
	do{
		if(ptr->seq == seqNum)
			return ptr;
		ptr = ptr->right;
	}while(ptr != end->right);
	return NULL;
}
/*
* This function will remove all the rodes that have sequence number less than ack
* This function should be called when an ack is received on the select
* @ ack - is a unsigned uint32_t tha represents the ACK that was received on the server
* @ return - is the number of elements in the NODE that received an ACK in the process
*/ 
int removeNodesContents(uint32_t ack){
	ServerBufferNode * ptr = start;
	int count = 0;
	do{
		if(ptr->seq < ack && ptr->occupied == 1){
			count++;
			ptr->seq = 0;
			ptr->occupied = 0;
			ptr->ackCount = 0;
			ptr->ts = 0;
			ptr->dataPayload = NULL;
			//What should i do with MsgHdr 
		}
		ptr = ptr->right;
	} while(ptr != end->right);
	start = ptr->right;
	return count;
}

/*
* This function will increment the ACK count in the ACK Received field 
* @ackRecv this is the ACK that came in 
*/
void updateAckField(int ackRecv){
	ServerBufferNode * ptr = start;
	int count = 0;
	do{
		if(ptr->seq == ackRecv && ptr->occupied == 1){
			ptr->ackCount++;
			//if acKcount is 3 we should fast retransmit
			//TODO
			return;
		}
		ptr = ptr->right;
	} while(ptr != end->right);
}
int addNodeContents(int seqNum, MsgHdr * data){
	if(availableWindowSize() <= 0)
		return -1 ;

	ServerBufferNode * ptr = end->right;
	if(ptr != 0)
		return -1;//this should never happen of we maintain the window properly

	ptr->seq = seqNum;
	ptr->ackCount = 0;
	ptr->occupied = 1;//symbolizes that this buffer is occupied
	ptr->dataPayload = data;//assign the data payload that is going to be maintaine

	struct timeval tv;
	if(gettimeofday(&tv, NULL) != 0){
		printf("Error getting time to set to packet.\n");
		return -1;
	}

	ptr->ts = ((tv.tv_sec * 1000) + tv.tv_usec / 1000); //this will get the timestamp in msec
	return 1 ;
}
int getWindowSize(){
	int count = 0;
	ServerBufferNode * ptr =cHead;
	do{
		count++;
		ptr = ptr->right;
	}while(ptr!= cHead);
	return count;
}
int main(){
	//allocateCircularBuffer(10);
	//int avail = availableWindowSize();
	//printf("avail: %d\n", avail);
	allocateCircularBuffer(10);
	int avail = availableWindowSize();
	printf("avail: %d\n", avail);
	avail = getWindowSize();
	printf("window Size %d \n", avail);
	ServerBufferNode *a = findSeqNode(0);
	if(a == NULL){
		printf("NULL\n");
	}
	printf("valid\n");

	//allocateCircularBuffer(1);
	//avail = availableWindowSize();
	//printf("avail: %d\n", avail);
	//printf("avail: %d\n", avail);
	//printf("%d\n", sizeof(MsgHdr));
	//printf("%d\n", sizeof(ServerBufferNode));
}