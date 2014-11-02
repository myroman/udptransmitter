#include <stdio.h>
#include <sys/stat.h>
#include "unpifiplus.h"
#include "dtghdr.h"
#include "fileChunking.h"

int chunkFileOpts(char * fileName, char **buffer, int numChunks, int chunkSize, int lastChunkRem) {
	int i;
	FILE* ftpFile = fopen(fileName, "r");
	size_t ret;
	for(i = 0; i < numChunks - 1; i++){
		buffer[i] = malloc(chunkSize);
		ret = fread(buffer[i], chunkSize, 1, ftpFile);	
	}
	
	if (lastChunkRem == 0) {
		buffer[i] = malloc(chunkSize);
		ret = fread(buffer[i], chunkSize , 1, ftpFile);
	}
	else {
		buffer[i] = malloc(lastChunkRem);			
		ret = fread(buffer[i], lastChunkRem , 1, ftpFile);
	}	
	
	if (fclose(ftpFile)!=0) {
		return 0;
	}	
	return 1;
}

int sizetoAllocate(char* fn, int * dataSize, int * numberOfSegments, int * rem) {
	char* fileName = fn;
	struct stat st;
	if((stat(fileName, &st)) != 0){
		printf("Error encountered when getting file statistics. Errno %s.\n.", strerror(errno));
		exit(0);
	}

	int fileSize = st.st_size;
	int dSize = getDtgBufSize();	
	*rem = fileSize % dSize;
	int numSegs;
	if(*rem == 0)
		numSegs = fileSize / dSize;
	else
		numSegs = (fileSize / dSize) + 1;

	printf("File size %d. Chunk Size %d Number of Segments %d.\n", fileSize, dSize, numSegs);

	*dataSize = dSize;
	*numberOfSegments = numSegs;
	return 0;
}

char** chunkFile(char* fileName, int* numChunks, int* lastChunkRem) {
	int chunkSize;
	sizetoAllocate(fileName, &chunkSize, numChunks, lastChunkRem);
	
	char ** charBuffer = malloc(sizeof(char*)*(*numChunks));
	chunkFileOpts(fileName, charBuffer, *numChunks, chunkSize, *lastChunkRem);

	return charBuffer;
}