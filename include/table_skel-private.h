#include <zookeeper/zookeeper.h>

struct ZKServerConnection {
    char* ip_addr;
    char* port;
    int sockfd;
};

void connection_watcher(zhandle_t *zzh, int type, int state, const char *path, void *context);

void child_watcher(zhandle_t *wzh, int type, int state, const char *zpath, void *watcher_ctx);


int init_zookeeper_connection(char* server_port, char* zoo_ip_port);

int establish_zk_server_connection(struct ZKServerConnection *zkConnection);

struct ZKServerConnection* create_server_connection(char* address_port);

int connect_next_server(const char* zoo_root);

int connect_predecessor_server(const char *zoo_root);

int zk_send_msg(struct ZKServerConnection * zkConnection, struct _MessageT *msg);


