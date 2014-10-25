#include "unpifiplus.h"
//#include "unpifi.h"
#include <net/if_arp.h>
#include "get_ifi_info_plus.c"
#include "unp.h"

int getSocketsNumber() {
	//Find out how many sockets there are so we allocate that much memory.	
	struct ifi_info *ifi;
	int sockInfoLength = 0;
	for(ifi = get_ifi_info_plus(AF_INET, 1); ifi != NULL ; ifi= ifi->ifi_next){
			sockInfoLength++;
	}
	
	return sockInfoLength;
}

void getInterfaces(SocketInfo* sockets_info) {
	struct ifi_info *ifi;
	//unsigned char *ptr;
	struct sockaddr *sa;
	
	int i;
	for(i = 0, ifi = get_ifi_info_plus(AF_INET, 1); ifi != NULL ; ifi= ifi->ifi_next, i++){						
			//Create the sockaddr_in structure for the IP address
			char * ip_c = sock_ntop(ifi->ifi_addr, sizeof(struct sockaddr));
			in_addr_t ipaddr_bits = inet_addr(ip_c);
			struct in_addr ip_addr;// = {ipaddr_bits};
			ip_addr.s_addr = ipaddr_bits;
			
			//Create the sockaddr_in structure for the network mask address
			char * netmask_c= sock_ntop(ifi->ifi_ntmaddr, sizeof(struct sockaddr));
			in_addr_t netmask_bits = inet_addr(netmask_c);
			struct in_addr netmask_addr; //= {netmask_bits};
			netmask_addr.s_addr = netmask_bits;
	
			//Bitwise and the IP address and newtwork mask
			in_addr_t and_ip_netmask = ipaddr_bits & netmask_bits;
			//Create the sockaddr_in structure for the subnet
			struct in_addr subnet_addr;// = {and_ip_netmask};
			subnet_addr.s_addr = and_ip_netmask;    
						
			sockets_info[i].ip_addr = ip_addr;
			sockets_info[i].netmask_addr = netmask_addr;
			sockets_info[i].subnet_addr = subnet_addr;
			
	}	
}

int checkIfLocalNetwork(char* serverIp) {
	SocketInfo* clientSockets;
	int ifsNumber = getSocketsNumber();
	clientSockets = (SocketInfo*)malloc(sizeof(SocketInfo) * ifsNumber);
	getInterfaces(clientSockets);
	
	int i;
	for (i = 0;i<ifsNumber;++i) {
		printf("IP address: %s\n", inet_ntoa(clientSockets[i].ip_addr));
		printf("Network mask: %s\n", inet_ntoa(clientSockets[i].netmask_addr));
	}
	return 0;
}
