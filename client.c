#include "unpifiplus.h"
#include <stdio.h>
#include "ifs.h"

int main()
{
	InpCd* inputData = (InpCd*)malloc(sizeof(InpCd));	
	if (parseInput(inputData) != 0) {
		return 1;
	}
	
	//determine if the server and client is one the same network
	
	char* clientIp = (char*)malloc(MAX_INPUT);
	int isServerLocal = checkIfLocalNetwork(inputData->ipAddrSrv, clientIp);
	
	if (isServerLocal == 2) {		
		clientIp = "127.0.0.1";
		inputData->ipAddrSrv = "127.0.0.1";
	}
	printf("Client IP:%s\n", clientIp);
	printf("Server Ip:%s\n", inputData->ipAddrSrv);;
	printf("Check the server is local: %d\n", isServerLocal);
	
	return 0;
}


