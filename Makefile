CC = gcc
FLAGS =  -g -O2
CFLAGS = -I../../lib -g -O2 -D_REENTRANT -Wall
#CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib
LIBS = ../../libunp.a -lpthread

all: get_ifi_info_plus.o prifinfo_plus.o
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}
	
client: client.o
	${CC} ${FLAGS} -o client client.o ${LIBS}
client.o: client.c
	${CC} ${CFLAGS} -c client.c


get_ifi_info_plus: get_ifi_info_plus.o
	${CC} ${FLAGS} -o get_ifi_info_plus get_ifi_info_plus.o ${LIBS}
get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c 

clean:
	rm prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o
