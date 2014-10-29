#ifndef __cli_circ_buf_h
#define __cli_circ_buf_h

typedef struct clientBufferNode ClientBufferNode;//typeDef for the Clinet Info object

struct clientBufferNode{
    int occupied;					//Symbolizes if the node is occupied or not. 1 means occupied, 0 means not occupied
    uint32_t  seqNum;				//The squence number of the datagram residing in the node
    MsgHdr * dataPayload;			//The contents of the message read in the buffer 
    ClientBufferNode *right;		//Pointer to the right
    ClientBufferNode *left;			//Pointer to the left
};

int consumeBuffer(FILE * fPointer, ClientBufferNode * start, ClientBufferNode* end, ClientBufferNode* cHead);
int addDataPayload(uint32_t s, MsgHdr* dp, ClientBufferNode * cHead, ClientBufferNode* start, ClientBufferNode* end);
void updateInorderEnd(ClientBufferNode * end);
uint32_t getAckToSend();
int getWindowSize(ClientBufferNode * cHead);
int availableWindowSize(ClientBufferNode * cHead);
int allocateCircularBuffer(int numToAllocate, ClientBufferNode* cHead, ClientBufferNode* cTail, ClientBufferNode* start, ClientBufferNode* end);

#endif