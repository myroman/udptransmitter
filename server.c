#include "unpifiplus.h"
//#include "unpifi.h"
#include <net/if_arp.h>
#include "get_ifi_info_plus.c"
#include "unp.h"


//**************************** Socket Info Data Structure  *******************//
typedef struct socketInfo{ //Remeber that this object is typedefed
	int sockfd;
	struct sockaddr *ip_addr;
	struct sockaddr *net_mask;
	struct sockaddr *subnet_addr; 
} SocketInfo;

SocketInfo * sockets_info;
int sockInfoLength = 0; 
//We will malloc space for this later when we know how many sockes we have
//We will store this as a array of sockeInfo so we can index into them. No linked list
//**************************** Socket Info Data Structure  *******************//


//**************************** clientInfo Data Structure *****************//
/*
* Doubly linked list storing which clients are being serverd currently.
*/
typedef struct ClientInfo ClientInfo;//typeDef for the Clinet Info object
typedef struct ClientInfo{
	int pid;//Process ID used to remove from the DS when server is done serving client
	struct sockaddr *clientAddress;
	ClientInfo *right;
	ClientInfo *left;
} ClientInfo;

ClientInfo *headClient = NULL;
ClientInfo *tailClient = NULL;
/*
* This function will add a new client to the structure that maintains clients that it is serving
* Return 0 if it failed to add new client
* Return 1 if it successful when adding a new client
* Return -1 if Malloc fails
*/
int addClientStruct(int processId, struct sockaddr *cAddr){
	int ret = 0; //return value of the funtion 

	//Malloc the space for the new struct
	ClientInfo *newCli = (ClientInfo *) malloc(sizeof( ClientInfo ));
	
	//Check if Malloc return NULL pointer ie out of space
	if(newCli == NULL){
		ret = -1;
		return ret;
	}
	
	//Set the data members in the structure
	newCli-> pid = processId;
	newCli-> clientAddress = cAddr;
	
	//Setup the chain
	if(headClient == NULL && tailClient == NULL){
		headClient = newCli;
		tailClient = newCli;
		ret = 1;
	}
	else if(headClient != NULL && tailClient != NULL){
		//set the tail client right to the newCli and set newCli to the tail
		newCli->left = tailClient;
		tailClient->right = newCli;
		tailClient = newCli;
		ret = 1;
	}
	else{
		//Error this code should never be reached
	}
	return ret;//return the status of the function 
}

ClientInfo* findClientRecord(int processId){
	ClientInfo *ptr = headClient;
	while (ptr!= NULL){
		if(ptr->pid == processId){
			return ptr;
		}
	}
	return ptr;
}
/*
* This function will delete a client record from the linked list
* @ptr the client_struct that will be deleted
* Return 1 on successful deletion
* Return 0 on failed deletion 
*/
int deleteClientRecord(ClientInfo * ptr){
	int ret = 0;
	if(ptr->left == NULL && ptr->right == NULL){
		headClient = NULL;
		tailClient = NULL;
		free(ptr);
		ret = 1;
	}
	//Delete the head
	else if(ptr->left == NULL && ptr->right != NULL){
		headClient = ptr->right;
		headClient->left = NULL;
		free(ptr);
		ret = 1;
	}
	//Delete the tail
	else if(ptr->left != NULL && ptr->right == NULL){
		tailClient = ptr->left;
		headClient->right = NULL;
		free(ptr);
		ret = 1;
	}
	else if(ptr-> left != NULL && ptr->right != NULL){
		ptr->left->right = ptr->right;
		ptr->right->left = ptr->left;
		free(ptr);
		ret = 1;
	}
	else{
		//Shouldn't have
	}
	return ret;
}

//**************************** processedClient Data Structure *****************//


int main (int argc, char ** argv){
	int sockfd;
	struct ifi_info *ifi;
	unsigned char *ptr;
	struct arpreq arpreq;
	struct sockaddr *sa;

	//sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	//Find out how many sockets there are so we allocate that much memory.
	for(ifi = get_ifi_info_plus(AF_INET, 1); ifi != NULL ; ifi= ifi->ifi_next){
		sockInfoLength++;
	}
	sockets_info = malloc(sizeof(SocketInfo) * sockInfoLength);
	if(sockets_info == NULL){
		printf("Out of memory! Exiting Program.\n ");
		exit(0);
	}
	printf("%d\n", sockInfoLength);

	for(ifi = get_ifi_info_plus(AF_INET, 1); ifi != NULL ; ifi= ifi->ifi_next){
		if((sa = ifi->ifi_addr) != NULL){
			printf("%hu, %s\n", ifi->ifi_index,sock_ntop(ifi->ifi_addr, sizeof(struct sockaddr)));
		}
		
		//printf("HERE\n");
		//printf("%s\n", Sock_ntop(ifi->ifi_brdaddr, sizeof(struct sockaddr)));
		//printf("%s\n", sock_ntop(ifi->ifi_ntmaddr, sizeof(struct sockaddr)));
		printf("MTU: %d\n", (int)ifi->ifi_mtu);
		if(ifi->ifi_next == NULL){
			printf("EQUALS NULL\n");
		}else{
			printf("NOT NULL\n");
		}
	}
	exit(0);
}