#include "dtghdr.h"
#include "unp.h"
#include "clientCircularBuffer.h"

/*
ClientBufferNode * cHead = NULL; 	//cHead is used to setup the circular buffer
ClientBufferNode * cTail = NULL;	//cTail is used to setup the circular buffer
ClientBufferNode * start = NULL;	//start is used to symbolize where the consumer can start reading from. Only modified by consumer
ClientBufferNode * end = NULL;		//end is used to symbolize the last inorder segment received up until this point
*/
uint32_t currentAck;// = 0; //Never touch this variable. To access call getAckToSend() function.

int allocateCircularBuffer(int numToAllocate, ClientBufferNode* cHead, ClientBufferNode* cTail, ClientBufferNode* start, ClientBufferNode* end) {
	
	if(numToAllocate < 1) {
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

int availableWindowSize(ClientBufferNode * cHead){
	ClientBufferNode* ptr = cHead;
	int count = 0;
	do{
		if (ptr->occupied == 0)
			++count;
		ptr = ptr->right;
	}while(ptr!= cHead);
	return count;
}

int getWindowSize(ClientBufferNode * cHead){
	int count = 0;
	ClientBufferNode * ptr =cHead;
	do{
		count++;
		ptr = ptr->right;
	}while(ptr!= cHead);
	return count;
}

uint32_t getAckToSend(){
	return currentAck;
}

void updateInorderEnd(ClientBufferNode * end){
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

int addDataPayload(uint32_t s, MsgHdr* dp, ClientBufferNode * cHead, ClientBufferNode* start, ClientBufferNode* end) {
	if(availableWindowSize(cHead) == 0){
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

		if(ptr->occupied == 1){//if its occupied just return with 0
			return 0;
		}
		ptr->seqNum = s;
		ptr->dataPayload = dp;

	}
	updateInorderEnd(end);//this will update the pointer to the end
	return 1;//successfully added data
}

int consumeBuffer(FILE * fPointer, ClientBufferNode * start, ClientBufferNode* end, ClientBufferNode* cHead) {
	int count = 0;
	if(start->occupied == 0){
		return count;;//nothing to read yet
	}

	if(getWindowSize(cHead) >= 2){
		do{
			count++;
			//Write out the data to the File
			MsgHdr *mptr = start->dataPayload;
			char *toWrite = extractBuffFromHdr(*mptr);
			fwrite(toWrite, sizeof(toWrite), 1,  fPointer);
			// ********

			start->occupied = 0;
			free(start->dataPayload);
			start->dataPayload=NULL;
			start = start->right;
		}while(start != end->right);
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