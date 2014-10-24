#UNP = -I/home/tony/Documents/CSE533/unpv13e/lib/
UNP = -I../../lib/
CC = gcc
#LIBS = /home/tony/Documents/CSE533/unpv13e/libunp.a
LIBS = ../../libunp.a
FLAGS = -g -O2
#CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib
#CFLAGS = ${FLAGS} -I/home/tony/Documents/CSE533/unpv13e/lib
CFLAGS = ${FLAGS} -I../../lib

all: get_ifi_info_plus.o prifinfo_plus.o server client testClient
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c ${UNP}

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c ${UNP}

server: server.o
	${CC} ${FLAGS} -o server server.o ${LIBS}
server.o: server.c
	${CC} ${FLAGS} -c server.c server.c ${UNP}	
client: client.o input.o
	${CC} ${FLAGS} -o client client.o input.o ${LIBS}
client.o: client.c 
	${CC} ${FLAGS} -c client.c client.c ${UNP}	
input.o: input.c
	${CC} ${FLAGS} -c input.c input.c ${UNP}

testClient: testClient.o
	${CC} ${FLAGS} -o testClient testClient.o ${LIBS}
testClient.o: testClient.c
	${CC} ${FLAGS} -c testClient.c ${UNP}

clean:
	rm prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o server server.o 
	client client.o testClient testClient.o input.o
