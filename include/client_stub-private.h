#ifndef _CLIENT_STUB_PRIVATE_H
#define _CLIENT_STUB_PRIVATE_H
#include <zookeeper/zookeeper.h>
#include "client_stub.h"

struct rtable_t {
    char *server_address;
    int server_port;
    int sockfd;
};

int rtable_connect_zookeeper();

struct entry_t **rtable_get_table_tail();

int rtable_disconnect_head_tail();

void connection_watcher_client(zhandle_t * zzh, int type, int state, const char *path, void *context);

void connect_client_to_zk(char *host_port);

void child_watcher_client(zhandle_t * wzh, int type, int state, const char *zpath, void *watcher_ctx);


#endif