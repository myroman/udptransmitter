#include "unpifiplus.h"
//#include "unpifi.h"
#include <net/if_arp.h>
#include "get_ifi_info_plus.c"
#include "unp.h"


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
void
signalChildHandler(int signal){
        pid_t pid;
        int stat;
        pid = wait(&stat);
        printf("Server child handled: %d pid terminated\n", pid);
        ClientInfo *toDelete = findClientRecord(pid);
        if(deleteClientRecord(toDelete) == 1){
        	printf("Successfully delete client info record\n");
        }
        return;
}


int main (int argc, char ** argv){
        signal(SIGCHLD, signalChildHandler);
        //int sockfd;
        struct ifi_info *ifi;
        //unsigned char *ptr;
        struct arpreq arpreq;
        struct sockaddr *sa;
        int PORT = 50000;
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
                servaddr.sin_port = htons(PORT);

                if( (bind(sockfd, (SA*) &servaddr, sizeof(servaddr))) != 0){
                        printf("Error binding IP Address %s, Port number %d\n", inet_ntoa(ip_addr), PORT);
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
                if(select(maxfd, &rset, NULL, NULL, NULL) < 0){
                        
                        if(errno != EINTR){
                        	err_quit("Error setting up select on all interfaces.");
                        }
                }
                printf("Successfully setup Select.\n");
                for(i = 0; i < sockInfoLength; i++){
                        if(FD_ISSET(sockets_info[i].sockfd, &rset)){
                                //We found the socket that it matched on. Then we need to fork off the child process
                                //that will server the client from here. 
                                printf("SELECT read something\n");
                                struct sockaddr_in cliaddr;
                                int n;
                                socklen_t len;
                                char mesg[MAXLINE];
                                len = sizeof(cliaddr);
                                n = recvfrom(sockets_info[i].sockfd, mesg, MAXLINE, 0, (SA*)&cliaddr, &len);
                                printf("READ from Socket: %s\n", mesg);
                                char * temp= sock_ntop((SA*)&cliaddr, sizeof(struct sockaddr_in));
                                printf("Client IP %s, port  %d.\n", temp, ntohs(cliaddr.sin_port));
                                
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
	                                if((pid = fork()) == 0){
	                                	//Child server process stuff goes here
	                                	//printf("Child pid: %d\n", pid);

	                                	//Go through the sockets info data structure and close all the listening sockets
	                                	int j;
	                                	for(j = 0; j < sockInfoLength; j++){
	                                		if(i!= j){
	                                			close(sockets_info[j].sockfd);//
	                                			printf("Closed Socket at index %d\n", j);
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
