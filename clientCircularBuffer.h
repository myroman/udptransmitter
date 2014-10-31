#ifndef __cli_circ_buf_h
#define __cli_circ_buf_h

typedef struct clientBufferNode ClientBufferNode;//typeDef for the Clinet Info object

struct clientBufferNode{
    int occupied;					//Symbolizes if the node is occupied or not. 1 means occupied, 0 means not occupied
    uint32_t  seqNum;				//The squence number of the datagram residing in the node
    char* cptr;
    MsgHdr * dataPayload;			//The contents of the message read in the buffer 
    ClientBufferNode *right;		//Pointer to the right
    ClientBufferNode *left;			//Pointer to the left
};

/*
* This function is called when the consumer thread wakes up and wants to consume everything in the buffer
* @fPointer - this is the file pointer that we will write the contents of the dataPayload to. 
* @return - returns the number of nodes consumned
*/
//int consumeBuffer(FILE * fPointer, ClientBufferNode * start, ClientBufferNode* end, ClientBufferNode* cHead);
int consumeBuffer(FILE * fPointer);
/*
* This function should be called when a new datagram is read. This function will add it to the circular buffer
* @ s - uint32_t that will be the sequence number of the datagram received. This is of type int.
* @ dp - this is the dataPayload of the datagram received. This is of type MsgHdr
* @ return -1 failure, 1 success, 0 means the space is occupied not resetting data
*/
//int addDataPayload(uint32_t s, MsgHdr* dp, ClientBufferNode * cHead, ClientBufferNode* start, ClientBufferNode* end);
int addDataPayload(uint32_t s, char* c ,MsgHdr* dp);
/*
* This function will advance the end pointer to the last inorder datagram. Also update the currentAck
*/
//void updateInorderEnd(ClientBufferNode * end);
void updateInorderEnd();

/*
* This function will give you the number to ack back to the server. Should be what tail is pointed to in essence
*/
uint32_t getAckToSend();
/*
* This will give you the total size of the circular buffer
*/
//int getWindowSize(ClientBufferNode * cHead);
int getWindowSize();
/*
* This function will give you the number of free spots on the circular buffer.
* Use the return value for the advertised sending window 
* @ return the number of available slots in the  circular buffer
*/
//int availableWindowSize(ClientBufferNode * cHead);
int availableWindowSize();

/*
* This function should be called ONCE. When the client starts up. 
* @ numToAllocate this integer indicates how many nodes our buffer will hold
* @ return There are two return values from this funtion
* 			-1 if the allocation failed (ie Malloc failed) or inproper variable passed
* 			1 if successful allocation occurs
*/
//int allocateCircularBuffer(int numToAllocate, ClientBufferNode* cHead, ClientBufferNode* cTail, ClientBufferNode* start, ClientBufferNode* end);
int allocateCircularBuffer(int numToAllocate);

#endif