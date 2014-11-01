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
	//printf("\n\n\ngonna memcpy %s\n", buf);
	//memcpy(data[1].iov_base, buf, bufSize);
	data[1].iov_base = buf;
	data[1].iov_len = bufSize;
	//printf("\n\n\ncontents:%s\n\n\n",extractBuffFromHdr(*msg));
}
void fillHdr2(DtgHdr* hdr, MsgHdr* msg, void* buf, size_t bufSize) {
	fillHdr(hdr, msg, buf, bufSize, NULL, 0);
}
char* extractBuffFromHdr(MsgHdr msg) {
	//printf("In ExtractBuferFromHdr: %s\n", msg.msg_iov[1].iov_base);
	return msg.msg_iov[1].iov_base;
	//return msg.msg_iov[1].iov_base;
}
DtgHdr* getDtgHdrFromMsg(MsgHdr* msg) {
	return msg->msg_iov[0].iov_base;
}
size_t getDtgBufSize() {	
	return (MAX_DTG_SIZE - sizeof(struct dtghdr));
}