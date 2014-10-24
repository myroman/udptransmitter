#UNP = -I/home/tony/Documents/CSE533/unpv13e/lib/
UNP = -I../../lib/
CC = gcc
#LIBS = /home/tony/Documents/CSE533/unpv13e/libunp.a
LIBS = ../../libunp.a
FLAGS = -g -O2
#CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib
#CFLAGS = ${FLAGS} -I/home/tony/Documents/CSE533/unpv13e/lib
CFLAGS = ${FLAGS} -I../../lib

all: get_ifi_info_plus.o prifinfo_plus.o server testClient testClient2
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c ${UNP}

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c ${UNP}

server: server.o
	${CC} ${FLAGS} -o server server.o ${LIBS}
server.o: server.c
	${CC} ${FLAGS} -c server.c server.c ${UNP}	

testClient: testClient.o
	${CC} ${FLAGS} -o testClient testClient.o ${LIBS}
testClient.o: testClient.c
	${CC} ${FLAGS} -c testClient.c ${UNP}

testClient2: testClient2.o
	${CC} ${FLAGS} -o testClient2 testClient2.o ${LIBS}
testClient2.o: testClient2.c
	${CC} ${FLAGS} -c testClient2.c ${UNP}

clean:
	rm prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o server server.o testClient testClient.o testClient2 testClient2.o
