include Make.defines

all: get_ifi_info_plus.o server client

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c ${UNP}

server: server.o input.o dtghdr.o fileChunking.o utils.o rtt.o
	${CC} ${FLAGS} -o server server.o  input.o dtghdr.o fileChunking.o utils.o rtt.o ${LIBS}
server.o: server.c
	${CC} ${FLAGS} -c server.c ${UNP}	
client: client.o input.o ifs.o dtghdr.o utils.o
	${CC} ${FLAGS} -o client client.o input.o ifs.o dtghdr.o utils.o -lm ${LIBS}
client.o: client.c 
	${CC} ${FLAGS} -c client.c -lm ${UNP}	
input.o: input.c
	${CC} ${FLAGS} -c input.c ${UNP}
dtghdr.o: dtghdr.c
	${CC} ${FLAGS} -c dtghdr.c ${UNP}
fileChunking.o: fileChunking.c
	${CC} ${FLAGS} -c fileChunking.c ${UNP}
utils.o: utils.c
	${CC} ${FLAGS} -c utils.c ${UNP}
rtt.o: rtt.c
	${CC} ${FLAGS} -c rtt.c ${UNP}

clean:
	rm *.o server client