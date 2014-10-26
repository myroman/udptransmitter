#ifndef __dth_hdr_h
#define __dth_hdr_h

struct dtghdr {
	uint32_t seq;	/* sequence # */
	uint32_t ack;
	uint32_t ts;		/* timestamp when sent */	
};

typedef struct dtghdr DtgHdr;

int fillHdr(DtgHdr* hdr, void* buf, size_t bufSize, const void* addr, socklen_t addrSize);

#endif /*__dth_hdr_h*/