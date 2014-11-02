int chunkFileOpts(char * fn, char **buffer, int numChunks, int chunkSize, int isDiv);
int sizetoAllocate(char* fn, int * dataSize, int * numberOfSegments, int * rem);
char** chunkFile(char* fileName, int* numChunks, int* lastChunkRem);