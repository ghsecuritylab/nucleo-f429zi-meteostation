#ifndef NETWORKING_USER_H
#define NETWORKING_USER_H

#include "lwip/tcp.h"
#include "lwip/apps/sntp.h"

#define MTU_SIZE 1500

void initialize_sntp(void);
void tcp_server_init(void);

#endif //NETWORKING_USER_H