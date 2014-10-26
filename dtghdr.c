#include "dtghdr.h"

void fillHdr(DtgHdr* hdr, MsgHdr* msg, void* buf, size_t bufSize, SA* sockAddr, socklen_t sockAddrSize) {
	msg->msg_name = sockAddr;
	msg->msg_namelen = sockAddrSize;
	
	struct iovec* data = (struct iovec*)malloc(sizeof(struct iovec) * 2);	
	msg->msg_iov = data;
	msg->msg_iovlen = 2;
	
	data[0].iov_base = hdr;
	data[0].iov_len = sizeof(struct dtghdr);
	data[1].iov_base = (char*)malloc(bufSize);
	//printf("gonna memcpy\n");
	//memcpy(data[1].iov_base, buf, bufSize);
	data[1].iov_base = buf;
	data[1].iov_len = bufSize;

}
void fillHdr2(DtgHdr* hdr, MsgHdr* msg, void* buf, size_t bufSize) {
	fillHdr(hdr, msg, buf, bufSize, NULL, 0);
}