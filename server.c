#include "unpifiplus.h"
//#include "unpifi.h"
#include <net/if_arp.h>
#include "get_ifi_info_plus.c"
#include "unp.h"
#include "dtghdr.h"
#include "fileChunking.h"
#include <setjmp.h>
#include "unprtt.h"
static void sig_alarm(int signo);
static void sig_alarm2(int signo);
static sigjmp_buf jmpbuf;
static sigjmp_buf jmpbuf2;
static struct rtt_info   rttinfo;
static int  rttinit = 0;
int resolveSockOptions(int sockNumber, struct sockaddr_in cliaddr);
int sendNewPortNumber(int sockfd, MsgHdr* pmsg, int lastSeqH, int newPort, struct sockaddr_in* cliaddr, size_t cliaddrSz);
int receiveThirdHandshake(int * listeningFd, int * connectionFd, MsgHdr * msg);
int startFileTransfer(char* fileName, int fd, int sockOpts, int* lastSeq, int cWinSize, int myWinSize);
int finishConnection(size_t sockfd, int sockOpts, int lastSeq);
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
struct ClientInfo{
    int pid;//Process ID used to remove from the DS when server is done serving client
    struct sockaddr_in clientAddress;
    ClientInfo *right;
    ClientInfo *left;
};

ClientInfo *headClient = NULL;
ClientInfo *tailClient = NULL;
/*
* This function will add a new client to the structure that maintains clients that it is serving
* Return 0 if it failed to add new client
* Return 1 if it successful when adding a new client
* Return -1 if Malloc fails
*/
int addClientStruct(int processId, struct sockaddr_in *cAddr){
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
    newCli-> clientAddress = *cAddr;
    
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
            ptr = ptr->right;
    }
    return ptr;
}

ClientInfo* findClientRecord2(in_addr_t ip, in_port_t port){
ClientInfo *ptr = headClient;
while (ptr != NULL){
	if(ptr->clientAddress.sin_addr.s_addr == ip && ptr->clientAddress.sin_port == port){
		printf("Match found in clients.\n");
		return ptr;
	}
	ptr = ptr-> right;
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


//************************Circular window buffer ************************//

ServerBufferNode * cHead = NULL;
ServerBufferNode * cTail = NULL;
ServerBufferNode * start = NULL;
ServerBufferNode * end = NULL;

uint32_t sequenceNumber = 1;// dont touch this variable
/*
* This function will allocate the number of nodes in the Circular buffer
* @ numToAllocate is the number of elements to allocate for
* @ return -1 error
*           1 success
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
        //printf("Here\n");
        for(i = 0; i < numToAllocate ; i++){
            if(i == 0){
                //printf("Here HEAD\n");
                cHead = &cptr[i];
                cptr[i].left = &cptr[numToAllocate-1];
                cptr[i].right = &cptr[i+1];
            }
            else if(i ==(numToAllocate -1)){
                //printf("Here TAIL\n");
                cTail = &cptr[i];
                cptr[i].right = &cptr[0];
                cptr[i].left = &cptr[i-1];
            }
            else{
                //printf("Here MIDDLE\n");
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
    //cTail = cHead;
    ServerBufferNode * ptr = cHead;
    do{
        ptr->occupied = 0;
        ptr->seq = 0;
        ptr->ts = 0;
        ptr->ackCount = 0;

        ptr = ptr->right; 
    }while(ptr!=cHead);
    return 1;
}

void printBufferContents(){
    ServerBufferNode * ptr = cHead;
    int count =0;
    printf("\n***** DUMP Buffer Contents: *****\n");
    do{
        printf("Node index: %d \n", count);
        printf("\tOccupied: %d, SequenceNumber: %d, AckCount: %d, Timestamp %d, PayloadPtr %d\n", ptr->occupied, ptr->seq, ptr->ackCount, ptr->ts, ptr->dataPayload);
        ptr = ptr-> right;
        count++;
    }while(ptr!= cHead);
    printf("\n**********************************\n");
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
    printf("removeNodeContents: start: %d, end: %d, ack: %u\n", start->seq, end->seq, ack);
    ServerBufferNode * ptr = start;
    //printBufferContents();
    int count = 0;
    do{
        if(start->seq < ack && start->occupied == 1){
            count++;
            start->seq = 0;
            start->occupied = 0;
            start->ackCount = 0;
            start->ts = 0;
            start->dataPayload = NULL;
            start->retransNumber = 0;
            start = start->right;
            //break;
            //What should i do with MsgHdr 
        }
        else{
            break;
        }
        
    } while(start != end->right);
    //printBufferContents();
    //printf("Available Window Size: %d\n", availableWindowSize());
    //start = ptr;
    //printf("removeNodeContents: start: %d, end: %d\n", start->seq, end->seq);
    //printf("Count is : %d\n", count);
    //printf("start occupied %d, start seq %d\n", start->occupied, start->seq);
    return count;
}

/*
* This function will increment the ACK count in the ACK Received field 
* @ackRecv this is the ACK that came in 
*/
int updateAckField(int ackRecv){
    ServerBufferNode * ptr = start;
    int count = 0;
    do{
        if(ptr->seq == ackRecv && ptr->occupied == 1){
            ptr->ackCount++;
            //if acKcount is 3 we should fast retransmit
            //TODO
            if(ptr->ackCount== 3){
                return 1;
            }
            return 0;;
        }
        ptr = ptr->right;
    } while(ptr != end->right);
    return -1;
}
uint32_t getSeqNumber(){
    ++sequenceNumber;
    return sequenceNumber;
}
/*
* This function will populate a need in the circular data with the data provided in the parameters
* @ seqNum is the sequence number of the packet to be inserted
* @ data is the MsgHdr object that the node in the buffer will send to the client
* @ return -1 is returned if we are out of space or error with time
*           1 is returned if successful insertion
*/
int addNodeContents(int seqNum, MsgHdr * data){
    if(availableWindowSize() <= 0)
        return -1 ;
    //printf("AddNodeContents: start: %d, end: %d\n", start->seq, end->seq);
    if(start->occupied == 0){
        start->seq = seqNum;
        start->ackCount = 0;
        start->occupied = 1;//symbolizes that this buffer is occupied
        start->dataPayload = data;//assign the data payload that is going to be maintaine
        start->retransNumber = 0;

        struct timeval tv;
        if(gettimeofday(&tv, NULL) != 0){
            printf("Error getting time to set to packet.\n");
            return -1;
        }

        start->ts = ((tv.tv_sec * 1000) + tv.tv_usec / 1000); //this will get the timestamp in msec
        end = start;
        return 1 ;
    }
    else{
        ServerBufferNode * ptr = end;
        if(end->occupied == 1)
            ptr = ptr->right;
        //if(ptr != 0)
        //  return -1;//this should never happen of we maintain the window properly

        ptr->seq = seqNum;
        ptr->ackCount = 0;
        ptr->occupied = 1;//symbolizes that this buffer is occupied
        ptr->dataPayload = data;//assign the data payload that is going to be maintaine
        ptr->retransNumber = 0;

        struct timeval tv;
        if(gettimeofday(&tv, NULL) != 0){
            printf("Error getting time to set to packet.\n");
            return -1;
        }

        ptr->ts = ((tv.tv_sec * 1000) + tv.tv_usec / 1000); //this will get the timestamp in msec
        end = ptr;
        return 1 ;
    }
}

/*
* This function will return the number of nodes in our circular buffer window.
* @ return the number of nodes in our circular buffer
*/
int getWindowSize(){
    //printf("IN get window size\n");
    int count = 0;
    ServerBufferNode * ptr =cHead;
    do{
        count++;
        ptr = ptr->right;
    }while(ptr!= cHead);
    //printf("countt: %d\n", count);
    return count;
}

/*
* This will return the number of nodes that not currently occupied
* @ return - the number of nodes that are free
*/
int availableWindowSize(){
    ServerBufferNode* ptr = cHead;
    int count = 0;
    do{
        if (ptr->occupied == 0){
            count++;
        }
        ptr = ptr->right;
    }while(ptr!= cHead);
    return count;
}

ServerBufferNode * getOldestInTransitNode(){
    printf("Oldest\n");
    //printf("start: %d %d\n", start->occupied, start->seq);
    ServerBufferNode *ptr;
    if(start->occupied == 0){
        ptr = NULL;
    }
    else{
        //printf("START\n");
        ptr = start;
    }
    return ptr;
}
//*****************************************************************************//

void signalChildHandler(int signal){
    pid_t pid;
    int stat;
    pid = wait(&stat);
    printf("Server child handled: %d pid terminated\n", pid);
    ClientInfo *toDelete = findClientRecord(pid);
    if(deleteClientRecord(toDelete) == 1){
    	printf("Successfully deleted client info record\n");
    }
    return;
}


int main (int argc, char ** argv){
    //Register sigchild handler to pick up on exiting child server processes
    signal(SIGCHLD, signalChildHandler);

    //Read from server.in file
    InpSd* inputData = (InpSd*)malloc(sizeof(InpSd));	
	if (parseInputServer(inputData) != 0) {
		return 1;
	}
	printf("Server Port: %d, Sliding window size %d\n", inputData->servPort, inputData->slidWndSize);
    //int sockfd;
    struct ifi_info *ifi;
    //unsigned char *ptr;
    struct arpreq arpreq;
    struct sockaddr *sa;
    fd_set rset;
    FD_ZERO(&rset);
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
    //printf("Socket Info Length: %d\n", sockInfoLength);
    int i;
    for(i = 0, ifi = get_ifi_info_plus(AF_INET, 1); ifi != NULL ; ifi= ifi->ifi_next, i++){
            printf("** Interface number %d **\n", (i+1));
            if((sa = ifi->ifi_addr) != NULL){
                    printf("IP Address: %s\n", sock_ntop(ifi->ifi_addr, sizeof(struct sockaddr)));
            }
            if((sa = ifi->ifi_ntmaddr) != NULL){
                    printf("Network Mask Address: %s\n", sock_ntop(ifi->ifi_ntmaddr, sizeof(struct sockaddr)));
            }
            
            //Create the sockaddr_in structure for the IP address
            char * ip_c = sock_ntop(ifi->ifi_addr, sizeof(struct sockaddr));
            in_addr_t ipaddr_bits = inet_addr(ip_c);
            struct in_addr ip_addr;
            ip_addr.s_addr = ipaddr_bits;
            //printf("testing solaris ip: %s\n", ip_c);
            //Create the sockaddr_in structure for the network mask address
            char * netmask_c= sock_ntop(ifi->ifi_ntmaddr, sizeof(struct sockaddr));
            in_addr_t netmask_bits = inet_addr(netmask_c);
            struct in_addr netmask_addr;
            netmask_addr.s_addr = netmask_bits;
            //printf("testing solaris netmask: %s\n", netmask_c);
            //Bitwise and the IP address and newtwork mask
            in_addr_t and_ip_netmask = ipaddr_bits & netmask_bits;
            //Create the sockaddr_in structure for the subnet
            struct in_addr subnet_addr;// = {and_ip_netmask};
            subnet_addr.s_addr = and_ip_netmask;    
            //Print out the dotted decimal of the subnet
            printf("Subnet Address: %s\n", inet_ntoa(subnet_addr));
            
            //Create the listening socket 

            int sockfd;
            struct sockaddr_in servaddr;

            sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            bzero(&servaddr, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr = ip_addr;
            servaddr.sin_port = htons(inputData->servPort);

            if( (bind(sockfd, (SA*) &servaddr, sizeof(servaddr))) != 0){
                    printf("Error binding IP Address %s, Port number %d\n", inet_ntoa(ip_addr), inputData->servPort);
                    exit(0);
            }
            printf("Sockfd: %d\n", sockfd);
            
            //Remove later for testing Datagram channel 
            /*int n;
            socklen_t len;
            char mesg[MAXLINE];
            for(;;){
                    len = sizeof(servaddr);
                    n = recvfrom(sockfd, mesg, MAXLINE, 0, (SA*)&servaddr, &len);
                    printf("READ from Socket: %s\n", mesg);
            }
            */

            //Insert the socket information into the socketsInfo struct
            SocketInfo sInfo;// = {sockfd, &ip_addr, &netmask_addr, &subnet_addr, &servaddr};
            sInfo.sockfd = sockfd;
            sInfo.ip_addr = ip_addr;
            sInfo.netmask_addr = netmask_addr;
            sInfo.subnet_addr = subnet_addr;
            sInfo.sockaddr = servaddr;

            //Add the socketInfo into the arry
            sockets_info[i] = sInfo;
    }

    //Setup Select on the Sockfd and select on all the sockfd in the sockets_info array
    //FD_ZERO(&rset);
    int maxfd;
    for(;;){
	//printf("HERE\n");
            //Set all she SOCK FDs to the set
            int maxfd;
            for(i = 0; i < sockInfoLength ; i++){
                    if(i == 0){
                            maxfd = sockets_info[i].sockfd;
                    }
                    
                    FD_SET(sockets_info[i].sockfd, &rset);
                    if(sockets_info[i].sockfd > maxfd){
                            maxfd = sockets_info[i].sockfd;
                    }
            }
            maxfd++;
	selectRetry:
            if(select(maxfd, &rset, NULL, NULL, NULL) < 0){
                    
                    if(errno != EINTR){
                    	err_quit("Error setting up select on all interfaces.");
                    }
					else{
						printf("EINTR caught on select. Retrying!\n");
						goto selectRetry;
					}
            }
            printf("Successfully setup Select.\n");
            for(i = 0; i < sockInfoLength; i++){
                    if(FD_ISSET(sockets_info[i].sockfd, &rset)){
                            //We found the socket that it matched on. Then we need to fork off the child process
                            //that will server the client from here. 
                            printf("%d: SELECT read something\n", getpid());

                            struct sockaddr_in cliaddr; 
                            //bzero(&cliaddr, sizeof(cliaddr));
                            char* buf = malloc(getDtgBufSize());
                            MsgHdr rmsg;
                            bzero(&rmsg, sizeof(rmsg));
                            DtgHdr rHdr;
                            bzero(&rHdr, sizeof(rHdr));
                            fillHdr(&rHdr, &rmsg, buf, getDtgBufSize(), (SA *)&cliaddr, sizeof(cliaddr));
                            int n;
                            if ((n = recvmsg(sockets_info[i].sockfd, &rmsg, 0)) == -1) {
                                err_quit("Error on recvmsg\n");
                            }
                            printf("seq=%d\n", rHdr.seq);
                            char* fileName = (char*)rmsg.msg_iov[1].iov_base;
                            int clientWndSize = ntohs(rHdr.advWnd);
                            //fileName[rmsg.msg_iov[1].iov_len] = 0;
                            printf("READ from Socket: %s\n", fileName);
                            printf("Client Window Size: %d\n", clientWndSize);
                            


                            char * temp; 
                            if( (temp = sock_ntop((SA*)&cliaddr, sizeof(struct sockaddr_in))) == NULL){
                                printf("Errno %s\n", strerror(errno));
                            }
                            printf("The client IP address %s, port:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                            //printf("Client IP %s, port  %d.\n", temp, cliaddr.sin_port);
                           
                            //Below is to test if find client works properly. Hard code the same IP to test
                            //cliaddr.sin_port= htons(50001);

                            //Try to find the client in the currently serving data structure
                            ClientInfo *findCli = findClientRecord2(cliaddr.sin_addr.s_addr, cliaddr.sin_port);
                            /*ClientInfo *findCli2 = findClientRecord(100);
                            if(findCli2!= NULL){
                            	printf("Found Client by pid\n");
                            }*/

                            if(findCli != NULL){
                            	printf("Client Found\n");
                            }
                            else{
                            	
                            	//For of the child process here
                            	int pid;
                                if((pid = fork()) == 0) {
									sleep(1);
                                	//Child server process stuff goes here
                                	//printf("Child pid: %d\n", pid);

                                	//Go through the sockets info data structure and close all the listening sockets
                                	int j, res;
                                	for(j = 0; j < sockInfoLength; j++){
                                		if(i!= j){
                                			close(sockets_info[j].sockfd);//
                                			printf("Closed Socket at index %d\n", j);
                                		}
                                	}

                                    int transSockOptions = resolveSockOptions(i, cliaddr);
                                	int transFd = socket(AF_INET, SOCK_DGRAM, 0);

									struct sockaddr_in transSock;                                    
                                    bzero(&transSock, sizeof(transSock));
									transSock.sin_family = AF_INET;
									transSock.sin_port = 0;//this will cause kernel to give ephemeral port
									transSock.sin_addr = sockets_info[i].ip_addr; //bind the socket to the ip address
									retryBind:	
									if((bind(transFd, (SA*) &transSock, sizeof(transSock))) == -1){//bind the new port
										if(errno != EINTR){
											printf("Binding new transfer socket error.  Errno: %s \n",  strerror(errno));
											exit(0);
										}
									}

									socklen_t transLen = sizeof(transSock);
									//struct sockaddr_in temp;
									if(getsockname(transFd,(SA*) &transSock, &transLen) == -1){
										printf("Error on getsockname\n");
										exit(0);
									}
									printf("IPServer Connection Socket: %s:%d\n", inet_ntoa(transSock.sin_addr), transSock.sin_port);

									//Call connect 
									if ((connect(transFd,(SA*) &cliaddr, sizeof(cliaddr))) != 0){
										printf("Error connecting to the client. Exiting...\n");
										exit(0);
									}
									printf("Connection is set up to client\n");									

                                    //sending 2nd handshake                                    
                                    MsgHdr smsg;
                                    bzero(&smsg, sizeof(smsg));
                                    if ((res = sendNewPortNumber(sockets_info[i].sockfd, &smsg, ntohs(rHdr.seq), transSock.sin_port, (struct sockaddr_in*)&cliaddr, sizeof(cliaddr))) == 0) {
                                        printf("Error when sending new port number\n");
                                        return;
                                    }

                                    //Here we need to receive 3rd handshake
                                    if((res = receiveThirdHandshake(&sockets_info[i].sockfd, &transFd, &smsg)) == 0) {
                                        printf("Didn't receive ACK from client. Exiting...\n");
                                    } else {
                                        int lastSeq;
                                    	if((res = startFileTransfer(fileName, transFd, transSockOptions, &lastSeq, clientWndSize, inputData->slidWndSize)) == 1) {
                                            if ((res=finishConnection(transFd, transSockOptions, lastSeq)) == 1) {
                                                
                                            }
                                        }
                                    }
                                	exit(0);
                                }
                                else{
                                	//parent server process goes here
                                	//Add child to the clients currently servering data structure
                                	printf("Child server process forked off pid=%d\n", pid);
                                	int retCheck = addClientStruct(pid, &cliaddr);
                                	if(retCheck == 1){
                                		printf("Successfully added client record\n");
                                	}
                                	else if( retCheck == 0){
                                		printf("Faild to add client record\n");
                                	}
                                	else{
                                		printf("Error malloc failed! Exiting...\n");
                                		exit(0);
                                	}
                                }
                            }
                    }
            }
    }
    exit(0);
}

int resolveSockOptions(int sockNumber, struct sockaddr_in cliaddr) {    
    //Step one if the server is localhost then we are done
    in_addr_t localHostConstant = inet_addr("127.0.0.1");
    if(sockets_info[sockNumber].ip_addr.s_addr == localHostConstant){
        printf("LocalHost Match found. IP server is local\n");
        return MSG_DONTROUTE;
    }
    //check if IPClient is local to any of the interfaces
    else if(sockets_info[sockNumber].subnet_addr.s_addr == (cliaddr.sin_addr.s_addr & sockets_info[sockNumber].netmask_addr.s_addr)){
        printf("IP Client : %s\n", inet_ntoa(cliaddr.sin_addr));
        printf("IPClient and IPServer are on the local using SO_DONTROOT OPTION\n");
        return MSG_DONTROUTE;
    }
    return 0;
}

int receiveThirdHandshake(int * listeningFd, int * connectionFd, MsgHdr * msg) {
	const int MAX_SECS_REPLY_WAIT = 5;
	const int MAX_TIMES_TO_SEND_FILENAME = 3;
	fd_set tset;
	FD_ZERO(&tset);
	int maxfd, i;
	struct timeval tv;
    
	for(i = 0; i < MAX_TIMES_TO_SEND_FILENAME; i++){
		tv.tv_sec = MAX_SECS_REPLY_WAIT;
    	tv.tv_usec = 0; 
		FD_ZERO(&tset);
		FD_SET(*connectionFd, &tset);
		maxfd = *connectionFd + 1;
		if(i != 0){
			//Send msg on connection socket
			if (sendmsg(*connectionFd, msg, 0) == -1) {
				printf("Error on sendmsg\n");
				return;
				printf("Error on sendmsg on connectionFd\n");
				continue;
				//return;
				printf("Error on sendmsg\n");
				return 0;
			}

			//Send msg on listening socket as well
			if (sendmsg(*listeningFd, msg, 0) == -1) {
				printf("Error on sendmsg on listenFd\n");
				continue;
			}
		}
		if((select(maxfd, &tset, NULL, NULL, &tv))){
			if(FD_ISSET(*connectionFd, &tset)){
                DtgHdr hdr;
                MsgHdr msg;
                fillHdr2(&hdr, &msg, NULL, 0);
                if (recvmsg(*connectionFd, &msg, 0) == -1) {
                    printf("Error when receiving 3rd handshake\n");
                    continue;
                }
                printf("We received the 3nd handshake, ack:%d, flags:%d\n", ntohs(hdr.ack), ntohs(hdr.flags));
				return 1;
			}
		}
	}
	return 0;
}

// 2nd handshake - sending the ephemeral port number
int sendNewPortNumber(int sockfd, MsgHdr* pmsg, int lastSeqH, int newPort, struct sockaddr_in* cliaddr, size_t cliaddrSz) {
    DtgHdr hdr;
    bzero(&hdr, sizeof(hdr));
    
    hdr.seq = htons(1);
    hdr.ack = htons(lastSeqH + 1);
    hdr.flags = htons(ACK_FLAG);
    
    printf("Sending 2nd handshake: ACK=%d, flag:%d\n", ntohs(hdr.ack), ntohs(hdr.flags));
    
    char* portStr = malloc(10);
    sprintf(portStr, "%d", newPort);
    
    fillHdr(&hdr, pmsg, portStr, getDtgBufSize(), (SA *)cliaddr, cliaddrSz);
    printf("Resolved after 2nd handshake client address %s:%d\n", inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
    if (sendmsg(sockfd, pmsg, 0) == -1) {        
        return 0;
    }
    printf("2nd handshake is sent\n");
    return 1;
}

/*int startFileTransfer(const char* fileName, int fd, int sockOpts, int* lastSeq, int cWinSize) {
    int numChunks, i, res, lastChunkRem;
    char** chunks = chunkFile(fileName, &numChunks, &lastChunkRem);
    DtgHdr hdr;
    MsgHdr msg;
    for (i=0; i < numChunks; ++i) {  
        //sleep(0.4);    
        //printf("sleep\n");
        bzero(&hdr, sizeof(hdr));    
        *lastSeq = i + 2;
        hdr.seq = htons(*lastSeq);
        if (i == numChunks -1) {
            hdr.chl = htons(lastChunkRem);
        }
        bzero(&msg, sizeof(msg));     
        fillHdr2(&hdr, &msg, chunks[i], getDtgBufSize());
        printf("Send chunk, SEQ=%d\n", ntohs(hdr.seq));
        res = sendmsg(fd, &msg, sockOpts);      
        printf("sendmsg returned %d\n", res);
        if (res <= 0) {
            return 0;
        }    
    }

    for(i = 0;i < numChunks; ++i) {
        bzero(&hdr, sizeof(hdr));  
        bzero(&msg, sizeof(msg));     
        fillHdr2(&hdr, &msg, NULL, 0);
        res = recvmsg(fd, &msg, 0);
        if (res == -1) 
            continue;
        //printf("Receive res=%d\n", res);
        int ack = ntohs(hdr.ack);
        int advWin = ntohs(hdr.advWnd);
        printf("Received ACK=%d, flags:%d, Advertised Window Size:%d\n", ack, ntohs(hdr.flags), ntohs(hdr.advWnd));
    }
    return 1;
    printf("File transfer complete\n");
}*/
int minimum(int cwin, int advWinSize){
    int availWinSize = availableWindowSize();
    //printf("Minimum: cwin:%d, advWinSize:%d, myspace: %d\n", cwin, advWinSize, availWinSize);
    //sleep(2);
    if(cwin <= advWinSize && cwin <= availWinSize){
        return cwin;
    }
    else if( availWinSize <= cwin && availWinSize <= advWinSize){
        return availWinSize;
    }
    else if(advWinSize <= cwin && advWinSize <= availWinSize){
        return advWinSize;
    }
    else{
        return -1;
    }
}
int minimum2(int cwin, int advWinSize){
    if(cwin <= advWinSize){
        return cwin;
    }
    else{
        return advWinSize;
    }
}
int startFileTransfer(char* fileName, int fd, int sockOpts, int* lastSeq, int cWinSize, int myWinSize) {
    //Malloc the sliding window size of the circular buffer
    printf("\n\n ***** Starting File Transfer *****\n\n");
    
    if(allocateCircularBuffer(myWinSize)!= 1){
        printf("Allocation of circular buffer failed.\n");
        return;
    }
    printf("Successfully allocated circular buffer.\n");

    if(cHead == NULL  || cTail == NULL || start == NULL || end == NULL){
        printf("a ptr == NULL\n");
    }
    //printBufferContents();
    int wsize = availableWindowSize();
    int tsize = getWindowSize();
    //printf("Total window size: %d, Available window size: %d\n", tsize, wsize);
    int numChunks, i, res, lastChunkRem, ret;
    char** chunks = chunkFile(fileName, &numChunks, &lastChunkRem);
    printf("Number of Chunks in the file: %d, Last chunk in file contains: %d\n", numChunks, lastChunkRem);
    int cwin = 1;//initiall cwin = 1
    int ssthresh = cWinSize;//initially sstresh = client's revc window
    int numToSend, sent, received;
    int ssFlag = 0;
    int cWinPercent= 0;
    int advWin = cWinSize;
    int totalSent = 0;
    int fastRetransmitFlag=0;
    int clientBuffFullFlag = 0;
    sigset_t sigset_alrm;
    sigemptyset(&sigset_alrm);
    sigaddset(&sigset_alrm, SIGALRM);
    MsgHdr *msg;
    DtgHdr *hdr;
    signal(SIGALRM, sig_alarm);
    i = 0;
    received = 0; 

    if (rttinit == 0) {
        rtt_init(&rttinfo);     /// first time we're called 
        rttinit = 1;
        rtt_d_flag = 1;
    }   

    int wasResending = 0;
    int delta = 0;            

    int maxAckReceived = 2;
    ServerBufferNode* tmpSbn;
    int numToAddToBuffer;
    int addedToBuffer;
    int probingFlag = 0;
    while(maxAckReceived < (numChunks+1)){
      FillBuffer:  
        numToAddToBuffer = minimum(cwin, advWin);
        printf("Adding to Buffer: CWIN: %d, SSTHRESH: %d, Clients Advertised Window: %d, My Buffer Space: %d\n", cwin, ssthresh,advWin, availableWindowSize());
        addedToBuffer = 0;
        sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);
        if(numToAddToBuffer != 0){
            while(addedToBuffer < numToAddToBuffer & i <numChunks){
                int toSendSeq = getSeqNumber();
                msg = malloc(sizeof(struct msghdr)); 
                hdr = malloc(sizeof(struct dtghdr));
                bzero(hdr, sizeof(struct dtghdr));
                bzero(msg, sizeof(struct msghdr));
                hdr->seq = htons(toSendSeq);//set the sequence number
                hdr->flags = htons(SYN_FLAG);//set the flags field
                if (i == numChunks -1) {
                    hdr->chl = htons(lastChunkRem);
                }
                fillHdr2(hdr, msg, chunks[i], getDtgBufSize());
                
                ret =addNodeContents(toSendSeq, msg);
                //res = sendmsg(fd, msg, sockOpts);
                ++addedToBuffer;
                ++i;    
            }
        }
        else{
            //Maybe just go to read here
        }
      SendPackets:
        numToSend = minimum2(cwin, advWin);
        if(numToSend > getWindowSize()){
            numToSend = getWindowSize();
        }
        printf("Sending Packets: CWIN: %d, SSTHRESH: %d, Clients Advertised Window: %d, My Buffer Space: %d\n", cwin, ssthresh,advWin, availableWindowSize());
        if(ssFlag == 0){
            printf("Slow Start Phase\n");
        }
        else{
            printf("Congestion Avoidance phase\n");
        }
        sent = 0;
        ServerBufferNode *ptr = start;
        int adjustedNumToRecv=0;
        //DtgHdr* hdr2 = getDtgHdrFromMsg(ptr->dataPayload);
        //hdr2->ts  = htons(rtt_ts(&rttinfo));
        printf("Number of packets sending:%d\n", numToSend);
        while(sent < numToSend){
            if(ptr->occupied == 1){
                //rtt_newpack(&rttinfo);
                sendmsg(fd, ptr->dataPayload, sockOpts);
                ++adjustedNumToRecv;
                printf("Sent packet with sequence number: %d\n", ptr->seq);
            }
            ptr = ptr->right;
            sent++;
        }
        sigprocmask(SIG_UNBLOCK, &sigset_alrm, NULL);
        if(sigsetjmp(jmpbuf,1) != 0){
            printf("Sigalarm Went off\n");
            printf("SigAlarm: CWIN: %d, SSTHRESH: %d, Clients Advertised Window: %d, My Buffer Space: %d\n", cwin, ssthresh,advWin, availableWindowSize());
            if(probingFlag == 1){
                printf("probingFlag == True\n");
                alarm(2);
                goto SendPackets;
            }
            else{
                printf("Actual time out\n");
                if(cwin / 2 > 0){
                    ssthresh = cwin / 2;
                }
                else{
                    ssthresh = 1;
                }
                cwin = 1;
                ssFlag = 0;
                cWinPercent = 0;
                alarm(2);
                goto SendPackets;
            }

        }

        
        if(availableWindowSize() == 0){
            int recvCount = 0;
            while(recvCount < getWindowSize()){   
                alarm(2);
                MsgHdr rmsg;
                DtgHdr rhdr;
                bzero(&rhdr, sizeof(rhdr));  
                bzero(&rmsg, sizeof(rmsg));     
                fillHdr2(&rhdr, &rmsg, NULL, 0);
                printf("Gonna receive IF...\n");
                res = recvmsg(fd, &rmsg, 0);
                printf("Received msg\n");
                alarm(0);
                int ack = ntohs(rhdr.ack);
                maxAckReceived = ack;
                printf("Received ACK=%d, flags:%d, Advertised Window Size:%d\n", ack, ntohs(rhdr.flags), ntohs(rhdr.advWnd));
                sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);
                int rValRemove = removeNodesContents(ack);
                recvCount += rValRemove;
                advWin = ntohs(rhdr.advWnd);
                if(rValRemove != 0){
                    if(ssFlag == 0){
                        ++cwin;
                        if(cwin>= ssthresh){
                            ssFlag = 1;
                            cWinPercent =0;
                        }
                    }
                    else{
                        ++cWinPercent;
                        if(cWinPercent % cwin == 0){
                            ++cwin;
                            cWinPercent = 0;
                        }
                    }
                }
                sigprocmask(SIG_UNBLOCK, &sigset_alrm, NULL);
            }
        }
        else{
            printf("In else part of code. My buffer is not full\n");
            
            if(advWin == 0 && adjustedNumToRecv == 0){
                printf("advwin is 0 but we will listen for one ack\n");
                adjustedNumToRecv =1;//we need to listen for one ack 
                while(1){
                    probingFlag = 1;
                    //alarm(2);
                    MsgHdr rmsg;
                    DtgHdr rhdr;
                    bzero(&rhdr, sizeof(rhdr));  
                    bzero(&rmsg, sizeof(rmsg));     
                    fillHdr2(&rhdr, &rmsg, NULL, 0);
                    printf("Gonna receive ADV WIN == 0...\n");
                    res = recvmsg(fd, &rmsg, 0);
                    printf("Received msg\n");
                    alarm(0);
                    int ack = ntohs(rhdr.ack);
                    maxAckReceived = ack;
                    printf("Received ACK=%d, flags:%d, Advertised Window Size:%d\n", ack, ntohs(rhdr.flags), ntohs(rhdr.advWnd));
                    sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);
                    int rValRemove = removeNodesContents(ack);
                    //amountReceived += rValRemove;
                    advWin = ntohs(rhdr.advWnd);
                    if(rValRemove != 0){
                        if(ssFlag == 0){
                            ++cwin;
                            if(cwin>= ssthresh){
                                ssFlag = 1;
                                cWinPercent =0;
                            }
                        }
                        else{
                            ++cWinPercent;
                            if(cWinPercent % cwin == 0){
                                ++cwin;
                                cWinPercent = 0;
                            }
                        }
                    }
                    sigprocmask(SIG_UNBLOCK, &sigset_alrm, NULL);
                    if(advWin > 0){
                        probingFlag = 0;
                        break;
                    }
                }
            }
            else{
                int amountReceived =0;
                while(amountReceived < adjustedNumToRecv){
                    alarm(2);
                    MsgHdr rmsg;
                    DtgHdr rhdr;
                    bzero(&rhdr, sizeof(rhdr));  
                    bzero(&rmsg, sizeof(rmsg));     
                    fillHdr2(&rhdr, &rmsg, NULL, 0);
                    printf("Gonna receive ELSE...\n");
                    res = recvmsg(fd, &rmsg, 0);
                    printf("Received msg\n");
                    alarm(0);
                    int ack = ntohs(rhdr.ack);
                    maxAckReceived = ack;
                    printf("Received ACK=%d, flags:%d, Advertised Window Size:%d\n", ack, ntohs(rhdr.flags), ntohs(rhdr.advWnd));
                    sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);
                    int rValRemove = removeNodesContents(ack);
                    amountReceived += rValRemove;
                    advWin = ntohs(rhdr.advWnd);
                    if(rValRemove != 0){
                        if(ssFlag == 0){
                            ++cwin;
                            if(cwin>= ssthresh){
                                ssFlag = 1;
                                cWinPercent =0;
                            }
                        }
                        else{
                            ++cWinPercent;
                            if(cWinPercent % cwin == 0){
                                ++cwin;
                                cWinPercent = 0;
                            }
                        }
                    }
                    sigprocmask(SIG_UNBLOCK, &sigset_alrm, NULL);
                }
            }
        }
    }
    printf("Finished File Transfer\n");
    return 1;
}

int finishConnection(size_t sockfd, int sockOpts, int lastSeq) {
    DtgHdr hdr;
    bzero(&hdr, sizeof(hdr));
    int toSendSeq = getSeqNumber();
    printf("FINISH CONNECTION: sequence number of ACK: %d\n", toSendSeq);
    hdr.seq = htons(toSendSeq);
    hdr.flags = htons(FIN_FLAG);
    signal(SIGALRM, sig_alarm2);
    MsgHdr msg;
    bzero(&msg, sizeof(msg));
    fillHdr2(&hdr, &msg, NULL, 0);
  sendagain2:
    printf("Finish connection, send seq:%d, flags:%d\n", ntohs(hdr.seq), ntohs(hdr.flags));
    int res = sendmsg(sockfd, &msg, sockOpts);
    if (res == -1) {
        printf("Error when sending a FIN\n");
        return 0;
    }
    printf("Waiting for ACK\n");
    
    printf("Set Alarm\n");
    if(sigsetjmp(jmpbuf2,1) != 0){
        printf("ABOUT TO REPEAT\n");
        goto sendagain2;
    }
    alarm(2);
    DtgHdr hdr2;
    MsgHdr msg2;
    
    int i;
    for(i = 0;i < MAX_NUMBER_TRIES_READ_SRV; ++i) {
        bzero(&hdr2, sizeof(hdr2));
        bzero(&msg2, sizeof(msg2));
        fillHdr2(&hdr2, &msg2, NULL, 0);
        if((res = recvmsg(sockfd, &msg2, sockOpts)) == -1) {
            printf("Error when reading final ACK\n");
            break;
            //continue;
        }
        int respFlags = ntohs(hdr2.flags);
        printf("Finally received ack:%d, flags:%d\n", ntohs(hdr2.ack), respFlags);
        if(respFlags == (6)){//FIN_FLAG | ACK_FLAG)) {
            printf("I got the final ack, gonna terminate the connection\n");
            return 1;
        } else {
            printf("It's not final ack, flags=%d\n", respFlags);
        }
    }
    printf("Exceeded the max number of tried to retrieve final ACK from client. Connection will be terminated\n");

    return 0;
}

static void sig_alarm(int signo){
    siglongjmp(jmpbuf,1);
}
static void sig_alarm2(int signo){
    printf("SIGALRM2 WENT OFF\n");
    siglongjmp(jmpbuf2,1);
}