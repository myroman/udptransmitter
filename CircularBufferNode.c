#include "dtghdr.h"
#include "unp.h"

typedef struct CircularBufferNode CircularBufferNode;//typeDef for the Clinet Info object
struct CircularBufferNode{
    int occupied;//Process ID used to remove from the DS when server is done serving client
    uint32_t  seq;
    uint32_t ackCount;
    uint32_t ts;
    MsgHdr * dataPayload;
    CircularBufferNode *right;
    CircularBufferNode *left;
};

CircularBufferNode * cHead = NULL;
CircularBufferNode * cTail = NULL;
CircularBufferNode * start = NULL;
CircularBufferNode * end = NULL;

int allocateCircularBuffer(int numToAllocate){
	if(numToAllocate < 1){
		printf("Invalid number of nodes to allocate...\n");
		return -1;
	}
	CircularBufferNode * cptr = malloc(sizeof(CircularBufferNode) * numToAllocate);
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
int availableWindowSize(){
	CircularBufferNode* ptr = cHead;
	int count = 0;
	do{
		if (ptr->occupied == 0)
			++count;
		ptr = ptr->right;
	}while(ptr!= cHead);
	return count;
}

CircularBufferNode * findSeqNode(int seqNum){
	CircularBufferNode * ptr = start;
	do{
		if(ptr->seq == seqNum)
			return ptr;
		ptr = ptr->right;
	}while(ptr != end);
	return NULL;
}
//Removes all the segments are less than the ack
//Returns the number of Nodes removed 
int removeNodesContents(int ack){
	CircularBufferNode * ptr = start;
	int count = 0;
	do{
		if(ptr->seq < ack){
			count++;
			ptr->seq = 0;
			ptr->occupied = 0;
			ptr->ackCount = 0;
			ptr->ts = 0;
			ptr->dataPayload = NULL;
			//What should i do with MsgHdr 
		}
		ptr = ptr->right;
	} while(ptr != end);
	return count;
}
void updateAckField(int ackRecv){
	CircularBufferNode * ptr = start;
	int count = 0;
	do{
		if(ptr->seq == ackRecv){
			ptr->ackCount++;
			return;
		}
		ptr = ptr->right;
	} while(ptr != end);
}
int addNodeContents(int seqNum, MsgHdr * data){
	if(availableWindowSize() <= 0)
		return -1 ;

	CircularBufferNode * ptr = end->right;
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
	CircularBufferNode * ptr =cHead;
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


	//allocateCircularBuffer(1);
	//avail = availableWindowSize();
	//printf("avail: %d\n", avail);
	//printf("avail: %d\n", avail);
	//printf("%d\n", sizeof(MsgHdr));
	//printf("%d\n", sizeof(CircularBufferNode));
}