#include "dtghdr.h"
#include "unp.h"

typedef struct ClientBufferNode ClientBufferNode;//typeDef for the Clinet Info object
struct ClientBufferNode{
    int occupied;//Process ID used to remove from the DS when server is done serving client
    uint32_t  seqNum;
    uint32_t ts;
    MsgHdr * dataPayload;
    ClientBufferNode *right;
    ClientBufferNode *left;
};

ClientBufferNode * cHead = NULL;
ClientBufferNode * cTail = NULL;
ClientBufferNode * start = NULL;
ClientBufferNode * end = NULL;
/*
* This function should be called ONCE. When the client starts up. 
* @ numToAllocate this integer indicates how many nodes our buffer will hold
* @ return There are two return values from this funtion
* 			-1 if the allocation failed (ie Malloc failed) or inproper variable passed
* 			1 if successful allocation occurs
*/
int allocateCircularBuffer(int numToAllocate){
	if(numToAllocate < 1){
		printf("Invalid number of nodes to allocate...\n");
		return -1;
	}
	ClientBufferNode * cptr = malloc(sizeof(ClientBufferNode) * numToAllocate);
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
* This function will give you the number of free spots on the circular buffer.
* Use the return value for the advertised sending window 
* @ return the number of available slots in the  circular buffer
*/
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

ClientBufferNode * findSeqNode(uint32_t seqNum){
	ClientBufferNode * ptr = start;
	do{
		if(ptr->seqNum == seqNum)
			return ptr;
		ptr = ptr->right;
	}while(ptr != end);
	return NULL;
}

int getWindowSize(){
	int count = 0;
	ClientBufferNode * ptr =cHead;
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
	ClientBufferNode *a = findSeqNode(0);
	if(a == NULL){
		printf("NULL\n");
	}
	printf("valid\n");

	//allocateCircularBuffer(1);
	//avail = availableWindowSize();
	//printf("avail: %d\n", avail);
	//printf("avail: %d\n", avail);
	//printf("%d\n", sizeof(MsgHdr));
	//printf("%d\n", sizeof(ClientBufferNode));
}

/*
* This function will advance the end pointer to the last inorder datagram
*/
void updateInorderEnd(){
	ClientBufferNode * ptr = end;
	do{
		if(end->occupied == 0){
			break;
		}
		ptr=ptr->right;// this will advance ptr
	}while(ptr != end); //go around this loop a max of once
	end = ptr;//set end to ptr;
}

/*
* This function should be called when a new datagram is read. This function will add it to the circular buffer
* @ s - uint32_t that will be the sequence number of the datagram received. This is of type int.
* @ dp - this is the dataPayload of the datagram received. This is of type MsgHdr
* @ return -1 failure, 1 success
*/
int addDataPayload(uint32_t s, MsgHdr* dp){
	if(availableWindowSize() == 0){
		return -1;
	}
	if(start->occupied == 0){
		start->seqNum = s;
		start->dataPayload = dp;
	}
	else{
		int difference, i;
		difference = s - start->seqNum;//this will get how many nodes to go forward 
		ClientBufferNode *ptr = start;
		for(i = 0; i < difference; i++){
			ptr = ptr->right;
		}
		ptr->seqNum = s;
		ptr->dataPayload = dp;

	}
	updateInorderEnd();//this will update the pointer to the end
	return 1;
}

int consumeBuffer(FILE * fPointer){
	int count = 0;
	if(start->occupied == 0){
		return count;;//nothing to read yet
	}

	if(getWindowSize() > 2){
		do{
			count++;
			//Write out the data to the File
			MsgHdr *mptr = start->dataPayload;
			char *toWrite = extractBuffFromHdr(*mptr);
			fwrite(toWrite, sizeof(toWrite), 1,  fPointer);
			// ********

			start->occupied = 0;
			start->dataPayload=NULL;
			start = start->right;
		}while(start != end->right);
	}
	else if( getWindowSize() == 1){

	}
	else{
		count++;
		//Write out the data to the File
		MsgHdr *mptr = start->dataPayload;
		char *toWrite = extractBuffFromHdr(*mptr);
		fwrite(toWrite, sizeof(toWrite), 1,  fPointer);
		// ********

		start->occupied = 0;
		start->dataPayload=NULL;
	}

}