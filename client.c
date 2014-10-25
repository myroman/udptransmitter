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
	int isServerLocal = checkIfLocalNetwork(inputData->ipAddrSrv);
	printf("Check the server is local: %d\n", isServerLocal);
	
	return 0;
}


