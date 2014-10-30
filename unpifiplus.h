/* Our own header for the programs that need interface configuration info.
   Include this file, instead of "unp.h". */

#ifndef __unp_ifi_plus_h
#define __unp_ifi_plus_h

#include        "unp.h"
#include        <net/if.h>

#define IFI_NAME        16                      /* same as IFNAMSIZ in <net/if.h> */
#define IFI_HADDR        8                      /* allow for 64-bit EUI-64 in future */

struct ifi_info {
  char    ifi_name[IFI_NAME];   /* interface name, null-terminated */
  short   ifi_index;                    /* interface index */
  short   ifi_mtu;                              /* interface MTU */
  u_char  ifi_haddr[IFI_HADDR]; /* hardware address */
  u_short ifi_hlen;                             /* # bytes in hardware address: 0, 6, 8 */
  short   ifi_flags;                    /* IFF_xxx constants from <net/if.h> */
  short   ifi_myflags;                  /* our own IFI_xxx flags */
  struct sockaddr  *ifi_addr;   /* primary address */
  struct sockaddr  *ifi_brdaddr;/* broadcast address */
  struct sockaddr  *ifi_dstaddr;/* destination address */

/*================ cse 533 Assignment 2 modifications =================*/

  struct sockaddr  *ifi_ntmaddr;  /* netmask address */

/*=====================================================================*/

  struct ifi_info  *ifi_next;   /* next of these structures */
};

#define IFI_ALIAS       1                       /* ifi_addr is an alias */

#define MAX_NUMBER_TRIES_READ_SRV 10

                                        /* function prototypes */
struct ifi_info *get_ifi_info_plus(int, int);
struct ifi_info *Get_ifi_info_plus(int, int);
void   free_ifi_info_plus(struct ifi_info *);

struct inputClientData
{
	char* ipAddrSrv;
	int srvPort;
	char* fileName;
	int slidWndSize;//in datagram units
	int rndSeed;
	float dtLossProb;
	int mean;
};

typedef struct inputClientData InpCd;
int parseInput(InpCd* dest);
int parseInt(char* s, int* outInt);

struct inputServerData
{
  int servPort;
  int slidWndSize;
};

typedef struct inputServerData InpSd;
int parseInputServer(InpSd* dest);

//**************************** Socket Info Data Structure  *******************//
typedef struct socketInfo{ //Remeber that this object is typedefed
        int sockfd;
        struct in_addr ip_addr;
        struct in_addr netmask_addr;
        struct in_addr subnet_addr; 
        struct sockaddr_in sockaddr;
} SocketInfo;
#endif  /* __unp_ifi_plus_h */
