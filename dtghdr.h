#include "unpifiplus.h"

#ifndef __dth_hdr_h
#define __dth_hdr_h

#define MAX_DTG_SIZE 512;

struct dtghdr {
	uint32_t seq;	/* sequence # */
	uint32_t ack;
	uint32_t ts;		/* timestamp when sent */	
	uint32_t flags; //We need to add this to sending
};

typedef struct dtghdr DtgHdr;
typedef struct msghdr MsgHdr;

void fillHdr(DtgHdr* hdr, MsgHdr* msg, void* buf, size_t bufSize, SA* sockAddr, socklen_t sockAddrSize);
void fillHdr2(DtgHdr* hdr, MsgHdr* msg, void* buf, size_t bufSize);
char* extractBuffFromHdr(MsgHdr msg); 
#endif /*__dth_hdr_h*/