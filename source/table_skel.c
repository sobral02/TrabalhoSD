#include "table_skel.h"
#include "table_skel-private.h"
#include "client_stub.h"
#include "client_stub-private.h"
#include "message.h"
#include "network_client.h"
#include "data.h"
#include "misc.h"
#include "entry.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "sys/time.h"
#include <zookeeper/zookeeper.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_reader = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_writer = PTHREAD_COND_INITIALIZER;

int active_readers = 0;
int waiting_writers = 0;
int active_writers = 0;

struct table_t *table;

struct statistics_t *stats;

static int is_connected;
static zhandle_t *zh;
static char *node_id;
static char *next_server;
struct ZKServerConnection *predecessor_server;
struct ZKServerConnection *zkConnection;
static char *watcher_ctx = "Zookeeper Data Watcher";
struct String_vector *children_list;
char *zookeeper_address;
const char *root_path;

struct ZKServerConnection *next_server_in_chain;

/* Inicia o skeleton da table.
 * O main() do servidor deve chamar esta função antes de poder usar a
 * função invoke(). O parâmetro n_lists define o número de listas a
 * serem usadas pela table mantida no servidor.
 * Retorna a table criada ou NULL em caso de erro.
 */
struct table_t *table_skel_init(int n_lists)
{
    if (n_lists < 1)
    {
        return NULL;
    }

    stats = (struct statistics_t *)malloc(sizeof(struct statistics_t));

    stats->counter = 0;
    stats->tempo = 0;
    stats->clientes = 0;

    table = table_create(n_lists);

    if (table == NULL)
    {
        return NULL;
    }

    return table;
}

/* Liberta toda a memória ocupada pela table e todos os recursos
 * e outros recursos usados pelo skeleton.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int table_skel_destroy(struct table_t *table)
{
    if (table == NULL)
    {
        return -1;
    }

    if (stats != NULL)
    {
        free(stats);
    }

    if (table_destroy(table) != 0)
    {
        return -1;
    }
    // Fechar conexão ZooKeeper se estiver conectado
    if (zh != NULL && is_connected)
    {
        zookeeper_close(zh);
        is_connected = 0;
        zh = NULL;
    }
    free(node_id);
    return 0;
}

/* Executa na table table a operação indicada pelo opcode contido em msg
 * e utiliza a mesma estrutura MessageT para devolver o resultado.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int invoke(MessageT *msg, struct table_t *table)
{

    if (msg == NULL || table == NULL)
    {
        return -1;
    }

    if (msg->c_type < 0 || msg->opcode < 0)
    {
        return -1;
    }

    int result;
    struct data_t *data;
    char **keys;
    // Tempo
    struct timeval start, end;

    gettimeofday(&start, NULL);
    long elapsed_us;

    switch (msg->opcode)
    {

    case MESSAGE_T__OPCODE__OP_STATS:

        comeca_ler();

        msg->opcode = MESSAGE_T__OPCODE__OP_STATS + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_STATS;

        msg->clientes = stats->clientes;
        msg->tempo = stats->tempo;
        msg->counter = stats->counter;

        termina_ler();

        break;

    case MESSAGE_T__OPCODE__OP_SIZE:

        comeca_ler();

        msg->opcode = MESSAGE_T__OPCODE__OP_SIZE + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        result = table_size(table);
        msg->result = result;

        if (result == -1)
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
            return -1;
        }

        stats->counter = stats->counter + 1;

        gettimeofday(&end, NULL);
        elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        stats->tempo += elapsed_us;

        termina_ler();
        break;

    case MESSAGE_T__OPCODE__OP_DEL:
        comeca_escrever();


        if (next_server_in_chain != NULL)
            if (zk_send_msg(next_server_in_chain, msg) != 0)
                printf("Erro ao enviar a mensagem para o proximo servidor");

        result = table_remove(table, msg->key);

        if (result == 0)
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_DEL + 1;
        }
        else
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
        }

        msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;

        stats->counter = stats->counter + 1;

        gettimeofday(&end, NULL);
        elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        stats->tempo += elapsed_us;

        termina_escrever();
        

        break;

    case MESSAGE_T__OPCODE__OP_GET:

        comeca_ler();

        data = table_get(table, msg->key);

        if (data != NULL)
        {
            msg->value.len = data->datasize;
            msg->value.data = malloc(data->datasize);

            if (msg->value.data == NULL)
            {
                data_destroy(data);
                return -1;
            }

            memcpy(msg->value.data, data->data, data->datasize);
            msg->opcode = MESSAGE_T__OPCODE__OP_GET + 1;
            msg->c_type = MESSAGE_T__C_TYPE__CT_VALUE;
            data_destroy(data);
        }
        else
        {
            msg->value.len = 0;
            msg->value.data = NULL;
            msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
            msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;
        }

        stats->counter = stats->counter + 1;

        gettimeofday(&end, NULL);
        elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        stats->tempo += elapsed_us;

        termina_ler();

        break;

    case MESSAGE_T__OPCODE__OP_PUT:

        comeca_escrever();

        if (next_server_in_chain != NULL)
            if (zk_send_msg(next_server_in_chain, msg) != 0)
                printf("Erro ao enviar a mensagem para o proximo servidor");

        if (msg->entry == NULL || msg->entry->key == NULL || msg->entry->value.data == NULL || msg->entry->value.len <= 0)
        {
            return -1;
        }

        int dataSize = msg->entry->value.len;
        void *data = malloc(dataSize);

        if (data == NULL)
        {
            return -1;
        }

        memcpy(data, msg->entry->value.data, dataSize);
        char *key = strdup((char *)msg->entry->key);

        if (key == NULL)
        {
            free(data);
            return -1;
        }

        struct data_t *new_data = data_create(dataSize, data);
        result = table_put(table, key, new_data);

        free(key);
        data_destroy(new_data);

        if (result == 0)
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_PUT + 1;
        }
        else
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
        }

        msg->c_type = MESSAGE_T__C_TYPE__CT_NONE;

        stats->counter = stats->counter + 1;

        gettimeofday(&end, NULL);
        elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        stats->tempo += elapsed_us;


        termina_escrever();


        break;

    case MESSAGE_T__OPCODE__OP_GETKEYS:

        comeca_ler();

        gettimeofday(&start, NULL);

        int size = table_size(table);
        keys = table_get_keys(table);

        if (keys == NULL)
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
            return -1;
        }

        msg->opcode = MESSAGE_T__OPCODE__OP_GETKEYS + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_KEYS;
        msg->n_keys = size;
        msg->keys = malloc(sizeof(char *) * size);

        if (msg->keys == NULL)
        {
            table_free_keys(keys);
            return -1;
        }

        for (int i = 0; i < size; i++)
        {
            msg->keys[i] = strdup((char *)keys[i]);

            if (msg->keys[i] == NULL)
            {
                while (i >= 0)
                {
                    free(msg->keys[i]);
                    i--;
                }
                free(msg->keys);
                table_free_keys(keys);
                return -1;
            }
        }

        table_free_keys(keys);

        stats->counter = stats->counter + 1;

        gettimeofday(&end, NULL);
        elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        stats->tempo += elapsed_us;

        termina_ler();

        break;

    case MESSAGE_T__OPCODE__OP_GETTABLE:
        comeca_ler();

        msg->opcode = MESSAGE_T__OPCODE__OP_GETTABLE + 1;
        msg->c_type = MESSAGE_T__C_TYPE__CT_TABLE;

        int tablesize = table_size(table);

        if (tablesize == -1)
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
            return -1;
        }

        msg->n_entries = tablesize;

        if (tablesize == 0)
        {
            break;
        }

        msg->entries = malloc(sizeof(EntryT *) * tablesize);

        if (msg->entries == NULL)
        {
            return -1;
        }

        keys = table_get_keys(table);

        if (keys == NULL)
        {
            msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
            free(msg->entries);
            return -1;
        }

        for (int i = 0; i < tablesize; i++)
        {
            struct data_t *dataT = table_get(table, keys[i]);

            if (dataT == NULL)
            {
                table_free_keys(keys);
                while (i >= 0)
                {
                    free(msg->entries[i]->key);
                    free(msg->entries[i]->value.data);
                    free(msg->entries[i]);
                    i--;
                }
                free(msg->entries);
                msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
                return -1;
            }

            int dataSize = dataT->datasize;
            void *data = malloc(dataSize);

            if (data == NULL)
            {
                table_free_keys(keys);
                while (i >= 0)
                {
                    free(msg->entries[i]->key);
                    free(msg->entries[i]->value.data);
                    free(msg->entries[i]);
                    i--;
                }
                free(msg->entries);
                data_destroy(dataT);
                msg->opcode = MESSAGE_T__OPCODE__OP_ERROR;
                return -1;
            }

            memcpy(data, dataT->data, dataSize);
            msg->entries[i] = malloc(sizeof(EntryT));
            entry_t__init(msg->entries[i]);
            msg->entries[i]->key = strdup(keys[i]);
            msg->entries[i]->value.len = dataSize;
            msg->entries[i]->value.data = data;

            data_destroy(dataT);
        }

        table_free_keys(keys);

        stats->counter = stats->counter + 1;

        gettimeofday(&end, NULL);
        elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        stats->tempo += elapsed_us;

        termina_ler();
        break;

    default:
        return -1;
    }

    return 0;
}

void increase_client()
{
    pthread_mutex_lock(&mutex);

    stats->clientes = stats->clientes + 1;

    pthread_mutex_unlock(&mutex);
}

void decrease_client()
{

    if (stats->clientes != 0)
    {
        pthread_mutex_lock(&mutex);
        stats->clientes = stats->clientes - 1;
        pthread_mutex_unlock(&mutex);
    }
}

void comeca_ler()
{
    pthread_mutex_lock(&mutex);
    while (active_writers > 0 || waiting_writers > 0)
    {
        pthread_cond_wait(&cond_reader, &mutex);
    }
    active_readers++;
    pthread_mutex_unlock(&mutex);
}

void termina_ler()
{

    pthread_mutex_lock(&mutex);
    active_readers--;
    if (active_readers == 0)
    {
        pthread_cond_broadcast(&cond_writer);
    }

    pthread_mutex_unlock(&mutex);
}

void comeca_escrever()
{

    pthread_mutex_lock(&mutex);
    waiting_writers++;
    while (active_readers > 0 || active_writers > 0)
    {
        pthread_cond_wait(&cond_writer, &mutex);
    }
    waiting_writers--;
    active_writers++;

    pthread_mutex_unlock(&mutex);
}
void termina_escrever()
{

    pthread_mutex_lock(&mutex);
    active_writers--;

    if (waiting_writers > 0)
    {
        pthread_cond_broadcast(&cond_writer);
    }
    else
    {
        pthread_cond_broadcast(&cond_reader);
    }

    pthread_mutex_unlock(&mutex);
}

/**
 * Watcher function for connection state change events
 */
void connection_watcher(zhandle_t *zzh, int type, int state, const char *path, void *context)
{
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE)
        {
            is_connected = 1;
        }
        else
        {
            is_connected = 0;
        }
    }
}

void child_watcher(zhandle_t *wzh, int type, int state, const char *zpath, void *watcher_ctx)
{
    struct String_vector *children_list = (struct String_vector *)malloc(sizeof(struct String_vector));
    if (state == ZOO_CONNECTED_STATE)
    {
        if (type == ZOO_CHILD_EVENT)
        {
            if (connect_next_server(root_path) != 0)
                return;
        }
        free(children_list);
    }
}

int init_zookeeper_connection(char *server_port, char *zoo_ip_port)
{
    /* Connect to ZooKeeper server */
    zookeeper_address = zoo_ip_port;
    root_path = "/chain";
    children_list = (struct String_vector *)malloc(sizeof(struct String_vector));

    zh = zookeeper_init(zoo_ip_port, connection_watcher, 2000, 0, NULL, 0);
    if (zh == NULL)
    {
        return -1;
    }

    struct ifaddrs *addrs, *tmp;

    getifaddrs(&addrs);
    tmp = addrs;
    char ip_address[15];
    int fd;
    struct ifreq ifr;

    while (tmp)
    {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET)

            if (strcmp(tmp->ifa_name, "lo") != 0)
            {
                fd = socket(AF_INET, SOCK_DGRAM, 0);
                ifr.ifr_addr.sa_family = AF_INET;
                memcpy(ifr.ifr_name, tmp->ifa_name, IFNAMSIZ - 1);
                ioctl(fd, SIOCGIFADDR, &ifr);
                close(fd);
                strcpy(ip_address, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
                printf("System IP Address is: %s\n", ip_address);
                break;
            }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);

    char *ip_port = calloc(strlen(ip_address) + strlen(server_port) + 2, sizeof(char));
    strcat(ip_port, ip_address);
    strcat(ip_port, ":");
    strcat(ip_port, server_port);

    int new_path_len1 = 1024;
    char *new_path1 = malloc(new_path_len1);

    if (ZNONODE == zoo_exists(zh, "/chain", 0, NULL))
    {
        if (ZOK != zoo_create(zh, "/chain", ip_port, strlen(ip_port) + 1, &(ZOO_OPEN_ACL_UNSAFE), 0, new_path1, new_path_len1))
        {
            fprintf(stderr, "Error creating znode from path %s!\n", new_path1);
            free(ip_port);
            free(new_path1);
            return -1;
        }
    }
    else
    {
        free(new_path1);
        new_path1 = "/chain";
    }
    char *node_path = calloc(120, sizeof(char));
    strcat(node_path, new_path1);
    strcat(node_path, "/node");

    int new_path_len = 1024;
    char *new_path = malloc(new_path_len);

    if (ZOK != zoo_create(zh, node_path, ip_port, strlen(ip_port) + 1, &(ZOO_OPEN_ACL_UNSAFE), ZOO_EPHEMERAL | ZOO_SEQUENCE, new_path, new_path_len))
    {
        fprintf(stderr, "Error creating znode from path %s!\n", node_path);
        free(ip_port);
        free(new_path1);
        free(node_path);
        free(new_path);

        return -1;
    }
    node_id = strdup(new_path);
    fprintf(stderr, "Ephemeral Sequencial ZNode created! ZNode path: %s\n", new_path);

    if (connect_predecessor_server(root_path) != 0)
    {
        fprintf(stderr, "Erro ao conectar ao servidor antecessor\n");
        free(ip_port);
        free(new_path1);
        free(node_path);
        free(new_path);
        return -1;
    }

    if (connect_next_server(root_path) != 0)
    {
        free(ip_port);
        free(new_path1);
        free(node_path);
        free(new_path);
        return -1;
    }

    free(ip_port);
    free(new_path);
    free(node_path);
    return 0;
}


int establish_zk_server_connection(struct ZKServerConnection *zkConnection)
{
    struct sockaddr_in server;
    int sockfd;

    // Cria socket TCP
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Erro ao criar socket TCP");
        return -1;
    }

    // Preenche estrutura server para estabelecer conexao
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(zkConnection->port));
    if (inet_pton(AF_INET, zkConnection->ip_addr, &server.sin_addr) != 1)
    {
        printf("Erro ao converter address\n");
        close(sockfd);

        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        close(sockfd);

        return -1;
    }

    zkConnection->sockfd = sockfd;
    return 0;
}

struct ZKServerConnection *create_server_connection(char *address_port)
{
    struct ZKServerConnection *connection = malloc(sizeof(struct ZKServerConnection));

    if (connection == NULL)
        return NULL;

    char *address_buffer = strtok((char *)address_port, ":");
    char **tokens = (char **)malloc(sizeof(char *) * 2);
    int tokenQuantity = 0;

    for (; address_buffer != NULL; tokenQuantity++)
    {
        tokens[tokenQuantity] = address_buffer;
        address_buffer = strtok(NULL, " ");
    }

    connection->ip_addr = tokens[0];
    connection->port = tokens[1];

    free(tokens);
    free(address_buffer);
    if (establish_zk_server_connection(connection) == 0)
        return connection;

    free(connection);
    
    return NULL;
}

/*Ver se existe um nó sucessor de entre os filhos de /chain, ou seja, com id mais alto a seguir ao próprio id;
 Se existir, obter os meta-dados desse servidor (ou seja, IP:porto) ligando-se a ele como next_server. 
 Caso contrário, next_server ficará NULL, o que significa que este servidor será a cauda da cadeia;*/
int connect_next_server(const char *root_path)
{
    if (zoo_wget_children(zh, root_path, child_watcher, watcher_ctx, children_list) != ZOK)
    {
        fprintf(stderr, "Error retrieving znode from path %s!\n", root_path);
        return -1;
    }
    fprintf(stderr, "\n=== znode listing === [ %s ]", root_path);
    for (int i = 0; i < children_list->count; i++)
    {
        fprintf(stderr, "\n(%d): %s", i + 1, children_list->data[i]);
    }
    fprintf(stderr, "\n=== done ===\n");
    
    char *max_path = strdup(node_id);

    for (int i = 0; i < children_list->count; i++)
    {

        char *path = strdup("/chain/");
        strcat(path, children_list->data[i]);

        if (strcmp(path, max_path) > 0)
        {
            max_path = calloc(strlen(children_list->data[i]) + 1, sizeof(char));
            strcat(max_path, "/chain/");
            strcat(max_path, children_list->data[i]);
            break;
        }
        free(path);
    }

    for (int i = 0; i < children_list->count; i++)
        free(children_list->data[i]);

    char *buffer = malloc(25);
    int buffer_len = 25;



    if (zoo_get(zh, max_path, 1, buffer, &buffer_len, NULL) != ZOK)
    {
        fprintf(stderr, "Error retrieving znode from path %s!\n", max_path);
        return -1;
    }

    if (strcmp(node_id, max_path) == 0)
    {
        next_server = NULL;
    }
    else
    {
        next_server = malloc(buffer_len);
        memcpy(next_server, buffer, buffer_len);

        next_server_in_chain = malloc(sizeof(struct ZKServerConnection));
        next_server_in_chain = create_server_connection(buffer);
    }

    return 0;
}
/*Ver se existe um nó antecessor de entre os filhos de /chain, ou seja, com id mais baixo antes do próprio id;
 Se existir, obter os meta-dados desse servidor (ou seja, IP:porto) ligando-se a ele de forma temporária 
 para obter uma cópia da tabela, usando a operação gettable. Para cada par <chave,valor> obtido,
  realizar uma operação put <chave,valor> na tabela local. No final, terminar a ligação ao servidor antecessor.*/
int connect_predecessor_server(const char *root_path)
{

    if (zoo_wget_children(zh, root_path, child_watcher, watcher_ctx, children_list) != ZOK)
    {
        fprintf(stderr, "Error retrieving znode from path %s!\n", root_path);
        return -1;
    }

    char *min_path = NULL;

    // Gets the min server id inferior to the current server id
    for (int i = 0; i < children_list->count; i++)
    {
        char *path = strdup("/chain/");
        strcat(path, children_list->data[i]);

        if (strcmp(path, node_id) < 0)
        {
            if (!min_path || strcmp(path, min_path) > 0)
            {
                if (min_path)
                    free(min_path);
                min_path = strdup(path);
            }
        }
        free(path);
    }

    // Free children list data
    for (int i = 0; i < children_list->count; i++)
        free(children_list->data[i]);

    if (!min_path)
    {
        return 0; // No predecessor
    }

    char *buffer = malloc(25);
    int buffer_len = 25;


    if (zoo_get(zh, min_path, 1, buffer, &buffer_len, NULL) != ZOK)
    {
        fprintf(stderr, "Error retrieving znode from path %s!\n", min_path);
        free(buffer);
        return -1;
    }
    // Connect to predecessor server
    predecessor_server = malloc(sizeof(struct ZKServerConnection));
    predecessor_server = create_server_connection(buffer);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*obter uma cópia da tabela, usando a operação gettable. Para cada par <chave,valor> obtido, realizar uma operação put <chave,valor> na tabela local.*/
char ip_port_str[50]; 

sprintf(ip_port_str, "%s:%s", predecessor_server->ip_addr, predecessor_server->port);
     // connect as client
    struct rtable_t* rtable = rtable_connect(ip_port_str);
    if (!rtable) return -1;
    struct entry_t** entries = rtable_get_table(rtable);
    if (!entries) return -1;
    int tab_size = rtable_size(rtable);
    if (tab_size==-1) return -1;
    rtable_disconnect(rtable);
    int ret = 0;
    for(int i = 0; i < tab_size;i++){
        ret = table_put(table,entries[i]->key,entries[i]->value);
        if (ret == -1){
            rtable_free_entries(entries);
            return ret;
        }
    }
    rtable_free_entries(entries);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    if (!predecessor_server)
    {
        fprintf(stderr, "Failed to connect to predecessor server.\n");
        return -1;
    }
    // Terminar a ligação ao servidor antecessor
    free(buffer);
    free(predecessor_server);
    return 0;
}
///////////////////////////////////////CHECKPOINT
//PARA/
///STOPP

int zk_send_msg(struct ZKServerConnection *zkConnection, struct _MessageT *msg)
{
    size_t msg_length = message_t__get_packed_size((const struct _MessageT *)msg);
    uint8_t *buffer = malloc(msg_length * sizeof(uint8_t));

    if (buffer == NULL)
        return -1;

    size_t packed_msg_length = message_t__pack(msg, buffer);

    int nbytes;
    if ((nbytes = write_all(zkConnection->sockfd, buffer, packed_msg_length)) != packed_msg_length)
    {
        perror("Erro ao enviar dados");
        close(zkConnection->sockfd);
        return -1;
    }
    free(buffer);

    return 0;
}