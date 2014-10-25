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

// Checks if otherIp is in the same network.
// return 1 if it is on the same network, 0 otherwise.
int checkIfLocalNetwork(char* otherIp) {
	SocketInfo* clientSockets;
	int ifsNumber = getSocketsNumber();
	clientSockets = (SocketInfo*)malloc(sizeof(SocketInfo) * ifsNumber);
	getInterfaces(clientSockets);
	
	int i;
	for (i = 0;i<ifsNumber;++i) {
		printf("Interface #%d:\n", i+1);
		printf("IP address: %s\n", inet_ntoa(clientSockets[i].ip_addr));
		printf("Network mask: %s\n", inet_ntoa(clientSockets[i].netmask_addr));
	}
	
	struct in_addr otherAddr;
	int otherIpNet = inet_aton(otherIp, &otherAddr);
	if (otherIpNet == 0) {
		printf("Error: the other ip can't be converted to network\n");
		return 0;
	}
	int coincidences = 0;
	int longestPrefix = 0, longestPrefIdx = 0;
	
	for(i = 0;i < ifsNumber; ++i) {
		struct in_addr otherSubnetmask;
		otherSubnetmask.s_addr = otherAddr.s_addr & clientSockets[i].netmask_addr.s_addr;
		
		printf("Others subnet for network mask #%d: %s\n", i + 1, inet_ntoa(otherSubnetmask));
		printf("Subnet #%d: %s\n", i + 1, inet_ntoa(clientSockets[i].subnet_addr));
		if (otherSubnetmask.s_addr == clientSockets[i].subnet_addr.s_addr) {
			++coincidences;
			if (clientSockets[i].netmask_addr.s_addr > longestPrefix) {
				longestPrefix = clientSockets[i].netmask_addr.s_addr;
				printf("A longer prefix found: %s\n", inet_ntoa(clientSockets[i].netmask_addr));
				longestPrefIdx = i;
				printf("Index of interface: %d\n",longestPrefIdx);
			}
		}		
	}
	return coincidences > 0;
}
