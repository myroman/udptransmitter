#include <sys/stat.h>
#include "unpifiplus.h"

int main(){
	char* fileName = "server.in";
	struct stat st;
	if((stat(fileName, &st)) != 0){
		printf("Error encountered when getting file statistics. Errno %s.\n.", strerror(errno));
		exit(0);
	}
	int fileSize = st.st_size;
	printf("File size %d.\n", fileSize);
	FILE* inputFile = fopen(fileName, "rt");
	if(inputFile == NULL){
		printf("Input file \"%s\" cannot be opened. Check if it exists.\n", fileName);
		exit(0);
	}


	fclose(inputFile);
	return 0;
}
