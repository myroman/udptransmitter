#include "unp.h"

int getSocketsNumber();
void getInterfaces(SocketInfo* sockets_info);
int checkIfLocalNetwork(const char* otherIp, char* matchedIp);
