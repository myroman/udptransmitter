#include <sys/stat.h>
#include "unpifiplus.h"
#include "dtghdr.h"
#define MAX_DATAGRAM_SIZE 512

int main(){
	//chunkFile("client.in");
	int dSize, numSegs, isDivisible;
	sizetoAllocate("test.in", &dSize, &numSegs, &isDivisible);
	printf ("dSize %d, numSegs %d\n", dSize, numSegs);
	char ** charBuffer = malloc(sizeof(char*) * numSegs);
	int i;
	for(i = 0; i < numSegs; i++){
		charBuffer[i] = malloc(dSize);
	}
	//char ** charBuffer = malloc(dSize * numSegs);
	chunkFile("test.in", charBuffer);
}

int chunkFile(char * fn, char **buffer){
	int chunkSize = MAX_DATAGRAM_SIZE - sizeof(struct dtghdr);
	char* fileName = fn;
	int dSize, numSegs, isDiv;
	sizetoAllocate(fn, &dSize, &numSegs, &isDiv);
	printf("DSIZE %d\n", dSize);
	int i;
	FILE* ftpFile = fopen(fn, "rt");
	if(isDiv == 0){
		printf("is div = 0\n");
		for(i = 0; i < numSegs; i++){
			//printf("index : %d\n", i);
			//char* b = (char*)malloc(dSize);
			printf("Chunk %d \n", i);
			while(fread(buffer[i], dSize , 1, ftpFile)){
				printf("%s\n", buffer[i]);
				break;
			}
		}
	}
	else{
		//printf("is div != 0\n");
		printf("numSegs %d idDiv %d dSize %d\n", numSegs, isDiv, dSize);
		for(i = 0; i < numSegs; i++){
			if(i == (numSegs-1)){
				printf("Chunk %d \n", i);
				while(fread(buffer[i], isDiv, 1, ftpFile)){
					printf("%s\n",buffer[i]);
					break;
				}
			}
			else{
				printf("Chunk %d \n", i);
				while(fread(buffer[i], dSize, 1, ftpFile)){
						printf("%s\n", buffer[i]);
						break;
				}
			}
		}
	}
	


	fclose(ftpFile);
	return 0;
}

int sizetoAllocate(char* fn, int * dataSize, int * numberOfSegments, int * rem){
	int dSize = 4;//MAX_DATAGRAM_SIZE - sizeof(struct dtghdr);
	char* fileName = fn;
	struct stat st;
	if((stat(fileName, &st)) != 0){
		printf("Error encountered when getting file statistics. Errno %s.\n.", strerror(errno));
		exit(0);
	}
	


	int fileSize = st.st_size;
	int isDivisible = fileSize % dSize;
	int numSegs;
	if(isDivisible == 0)
		numSegs = fileSize / dSize;
	else
		numSegs = (fileSize / dSize) + 1;

	printf("File size %d. Chunk Size %d Number of Segments %d.\n", fileSize, dSize, numSegs);

	*rem = isDivisible;
	*dataSize = dSize;
	*numberOfSegments = numSegs;
	return 0;
}
