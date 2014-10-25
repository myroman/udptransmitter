include Make.defines

all: get_ifi_info_plus.o prifinfo_plus.o server client testClient testClient2
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

testClient2: testClient2.o
	${CC} ${FLAGS} -o testClient2 testClient2.o ${LIBS}
testClient2.o: testClient2.c
	${CC} ${FLAGS} -c testClient2.c ${UNP}

clean:
	rm prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o server server.o client client.o testClient testClient.o input.o testClient2.o testClient2
