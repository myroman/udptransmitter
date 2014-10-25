#include "unpifiplus.h"

int parseInput(InpCd* dest) {
	char* fileName = "client.in";
	FILE* inputFile = fopen(fileName, "rt");
	if (inputFile == NULL) {
		printf("Input file \"%s\" cannot be opened. Check if it exists.\n", fileName);
		return 1;
	}
	char buf[80];
	int line = 0;
	while(fgets(buf, 80, inputFile) != NULL)
	{
		int length = strlen(buf);
		if (length<=0) 
			continue;
		if (line == 7)
			break;
		switch(line)
		{
			case 0:
				dest->ipAddrSrv = (char*)malloc(length);
				strcpy(dest->ipAddrSrv, buf);			
				break;
			case 1:
				dest->srvPort = (char*)malloc(length);
				strcpy(dest->srvPort, buf);
				break;
			case 2:
				dest->fileName = (char*)malloc(length);
				strcpy(dest->fileName, buf);			
				break;
			case 3:				
				if (parseInt(buf, &dest->slidWndSize) != 0) {
					printf("Couldn't parse slider window size\n");
					return 1;
				}		
				break;
			case 4:
				if (parseInt(buf, &dest->rndSeed) != 0) {
					printf("Couldn't parse the seed for random values generator\n");
					return 1;
				}
				break;
			case 5:
				if (parseInt(buf, &dest->dtLossProb) != 0) {
					printf("Couldn't parse loss probability\n");
					return 1;
				}
				break;
			case 6:
				if (parseInt(buf, &dest->mean) != 0) {
					printf("Couldn't parse mean\n");
					return 1;
				}
				break;
			default:
				break;
		}		
		++line;
	}
	fclose(inputFile);	
	return 0;
}

int parseInt(char* s, int* outInt) {
	int l = strlen(s) - 1;
	char* buf2=(char*)malloc(l);
	memcpy(buf2, s, l);
	int r = sscanf(s, "%d", outInt);
	free(buf2);
	
	return r != 1;//return 0 if r == 1
}
