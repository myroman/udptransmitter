#include "dtghdf.h"

int fillHdr(dtghdr* hdr, void* buf, size_t bufSize, const void* addr, socklen_t addrSize) {
	struct iovec data[2];

	msgrecv.msg_name = addr;
	msgrecv.msg_namelen = addrSize;
	msgrecv.msg_iov = data;
	msgrecv.msg_iovlen = 2;
	data[0].iov_base = &hdr;
	data[0].iov_len = sizeof(struct dtghdr);
	data[1].iov_base = buf;
	data[1].iov_len = bufSize;

	return 1;
}