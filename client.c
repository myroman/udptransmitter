#include "unpifiplus.h"
#include <stdio.h>

int main()
{
	InpCd* inputData = (InpCd*)malloc(sizeof(InpCd));	
	printf("SIze to alloc: %d\n", sizeof(InpCd));
	if (parseInput(inputData) != 0) {
		return 1;
	}
	printf("Ip: %s\n", inputData->ipAddrSrv);
	printf("Port: %s\n", inputData->srvPort);
	printf("SWS: %d\n", inputData->slidWndSize);
	
	return 0;
}


