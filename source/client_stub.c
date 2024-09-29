#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_stub.h"
#include "network_client.h"
#include "client_stub-private.h"
#include "sdmessage.pb-c.h"
#include "stats.h"
#include <zookeeper/zookeeper.h>
/* ZooKeeper Znode Data Length (1MB, the max supported) */
#define ZDATALEN 1024 * 1024

typedef struct String_vector zoo_string;

static zhandle_t *zh;
static int is_connected;
static char *watcher_ctx = "ZooKeeper Data Watcher";



/*
O cliente passa a ligar-se ao ZooKeeper e a dois servidores: o servidor head e o tail da cadeia de replicação.
Para tal, podem ser utilizadas duas cópias da estrutura rtable_t, uma para guardar os dados da ligação ao servidor head e
outra para guardar os dados da ligação ao servidor tail. Se existir apenas um servidor ativo, este será tanto head como tail
e os clientes terão duas ligações ao mesmo servidor.*/
struct rtable_t *head;
struct rtable_t *tail;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Funções da fase anterior inalteradas

/* Função para estabelecer uma associação entre o cliente e o servidor,
 * em que address_port é uma string no formato <hostname>:<port>.
 * Retorna a estrutura rtable preenchida, ou NULL em caso de erro.
 */
struct rtable_t *rtable_connect(char *address_port)
{
    if (address_port == NULL)
    { // Verifica se address_port é nulo
        return NULL;
    }
    // Alocação de memória para rtable
    struct rtable_t *rtable = (struct rtable_t *)malloc(sizeof(struct rtable_t));

    if (rtable == NULL)
    {

        perror("Erro ao alocar memória para a remote table (rtable_connect)");

        return NULL;
    }

    // Split para obter o endereço e porto
    char *address = strtok(address_port, ":");
    char *port_string = strtok(NULL, ":");

    // No caso de um deles ser NULL
    if (address == NULL || port_string == NULL)
    {

        printf("Porto ou servidor em formato inválido (rtable_connect): %s\n", address_port);
        free(rtable);

        return NULL;
    }

    // Convertemos o porto em int
    int server_port = atoi(port_string);

    if (server_port == 0)
    { // atoi deu return a 0, logo não converteu

        printf("Porto inválido (rtable_connect): %s\n", address_port);
        free(rtable);

        return NULL;
    }

    rtable->server_address = strdup(address);
    rtable->server_port = server_port;

    if (network_connect(rtable) == 0)
    { // Conexão com sucesso

        return rtable;
    }
    else
    { // Conexão sem sucesso, libertamos a memória de rtable

        free(rtable->server_address);
        free(rtable);

        return NULL;
    }
}

/* Termina a associação entre o cliente e o servidor, fechando a
 * ligação com o servidor e libertando toda a memória local.
 * Retorna 0 se tudo correr bem, ou -1 em caso de erro.
 */
int rtable_disconnect(struct rtable_t *rtable)
{

    if (rtable == NULL)
    { // Verificação de pointers
        return -1;
    }

    // Fechamos a socket e libertamos a memória ocupada por rtable
    network_close(rtable);
    free(rtable->server_address);
    free(rtable);
    return 0;
}

/* Retorna o número de elementos contidos na tabela ou -1 em caso de erro.
 */
int rtable_size(struct rtable_t *rtable)
{

    if (rtable == NULL)
    {
        return -1;
    }

    // Criar uma mensagem do tipo MESSAGE_T__OPCODE__OP_SIZE para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_SIZE;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_NONE;

    // Enviar a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(rtable, &messageT);

    if (response == NULL)
    {
        return -1; // Erro na comunicação com o servidor.
    }

    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_SIZE + 1)
    {
        // A operação foi realizada com sucesso.
        int size = response->result;
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return size;
    }
    else
    {

        // Caso contrário, algo inesperado aconteceu.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
}

/* Retorna um array de entry_t* com todo o conteúdo da tabela, colocando
 * um último elemento do array a NULL. Retorna NULL em caso de erro.
 */
struct entry_t **rtable_get_table(struct rtable_t *rtable)
{
    if (rtable == NULL)
    {
        return NULL;
    }

    // Cria uma mensagem do tipo MESSAGE_T__OPCODE__OP_DEL para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_GETTABLE;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_NONE;

    // Envia a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(rtable, &messageT);

    if (response == NULL)
    {
        return NULL; // Erro na comunicação com o servidor.
    }

    // Verifica a resposta do servidor.
    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_GETTABLE + 1)
    {
        // A operação foi realizada com sucesso.

        // Cria um array de entry_t * para armazenar os dados da tabela.
        int num_entries = response->n_entries;
        struct entry_t **table = (struct entry_t **)malloc((num_entries + 1) * sizeof(struct entry_t *));

        if (table == NULL)
        {
            message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
            return NULL;
        }

        // Copia os dados da resposta para o array de entry_t *.
        for (int i = 0; i < num_entries; i++)
        {

            char *key = strdup((char *)response->entries[i]->key);
            int size = response->entries[i]->value.len;
            char *data = strdup((char *)response->entries[i]->value.data);
            struct data_t *dataT = data_create(size, data);

            table[i] = entry_create(key, dataT);

            if (table[i] == NULL)
            {

                // Libertar a memória alocada até agora em caso de erro.
                for (int j = 0; j < i; j++)
                {
                    entry_destroy(table[j]);
                }

                free(table);
                message_t__free_unpacked(response, NULL); // Liberta a mensagem de resposta.
                perror("Erro ao alocar memória client_stub (rtable_get_table)");

                return NULL;
            }
        }

        table[num_entries] = NULL; // Último elemento NULL.

        message_t__free_unpacked(response, NULL); // Liberta a mensagem de resposta.
        return table;
    }
    else
    {

        // Liberta a mensagem de resposta.
        message_t__free_unpacked(response, NULL);
        return NULL;
    }
}

// ##############################################################################################################

int rtable_connect_zookeeper()
{
    //estava a dar conflito enquanto variaveis globais
    const char *zoo_root = "/chain";
    struct String_vector *children_list;


    
    children_list = (struct String_vector *)malloc(sizeof(struct String_vector));
    /* Get the list of children synchronously */
    zoo_wget_children(zh, zoo_root, child_watcher_client, watcher_ctx, children_list);
    if (ZOK != zoo_wget_children(zh, zoo_root, child_watcher_client, watcher_ctx, children_list))
    {
        fprintf(stderr, "Error retrieving znode from path %s!\n", zoo_root);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "\n=== znode listing === [ %s ]", zoo_root);
    for (int i = 0; i < children_list->count; i++)
    {
        fprintf(stderr, "\n(%d): %s", i + 1, children_list->data[i]);
    }

    fprintf(stderr, "\n=== done ===\n");
    char *max_server_path = calloc(50, sizeof(char));
    char *min_server_path = calloc(50, sizeof(char));
    strcat(max_server_path, "/chain/");
    strcpy(max_server_path + 7, children_list->data[0]); //+7 para saltar o /chain
    strcat(min_server_path, "/chain/");
    strcpy(min_server_path + 7, children_list->data[0]);

    // ir buscar o max e min server
    for (int i = 0; i < children_list->count; i++)
    {
        char *path = strdup("/chain/");
        strcat(path, children_list->data[i]);

        if (strcmp(path, max_server_path) > 0)
        {
            strcpy(max_server_path + 7, children_list->data[i]);
        }
        if (strcmp(path, min_server_path) < 0)
        {
            strcpy(min_server_path + 7, children_list->data[i]);
        }
        free(children_list->data[i]);
        free(path);
    }
    char *head_buffer = malloc(50);
    char *tail_buffer = malloc(50);
    int buffer_len = 50;


    if (ZOK != zoo_get(zh, max_server_path, 1, tail_buffer, &buffer_len, NULL))
    {
        fprintf(stderr, "Error retrieving znode from path %s!\n", max_server_path);
        exit(EXIT_FAILURE);
    }

    if (ZOK != zoo_get(zh, min_server_path, 1, head_buffer, &buffer_len, NULL))
    {
        fprintf(stderr, "Error retrieving znode from path %s!\n", max_server_path);
        exit(EXIT_FAILURE);
    }

    char *head_address = strtok((char *)head_buffer, ":");
    char **head_tokens = (char **)malloc(sizeof(char *) * 2);
    int tokenQuantity_head = 0;

    for (; head_address != NULL; tokenQuantity_head++)
    {
        head_tokens[tokenQuantity_head] = head_address;
        head_address = strtok(NULL, " ");
    }

    head = malloc(sizeof(struct rtable_t));

    head->server_address = head_tokens[0];
    head->server_port = atoi(head_tokens[1]);

    free(head_tokens);
    free(head_address);

    if (network_connect(head) != 0){
        printf("Erro na head");
    }

    char *tail_address = strtok((char *)tail_buffer, ":");
    char **tail_tokens = (char **)malloc(sizeof(char *) * 2);
    int tokenQuantity_tail = 0;

    // extrair os tokens o array inputTokens
    for (; tail_address != NULL; tokenQuantity_tail++)
    {
        tail_tokens[tokenQuantity_tail] = tail_address;
        tail_address = strtok(NULL, " ");
    }

    tail = malloc(sizeof(struct rtable_t));

    tail->server_address = tail_tokens[0];
    tail->server_port = atoi(tail_tokens[1]);

    free(tail_tokens);
    free(tail_address);

    if (network_connect(tail) != 0){
        printf("Erro na tail");
    }

    return 0;
}

/*
disconnectar tanto a head como a tail*/
int rtable_disconnect_head_tail()
{
    if (zh != NULL && is_connected)
    {
        zookeeper_close(zh);
        is_connected = 0;
        zh = NULL;
    }
    int disconnect_head = network_close(head);
    int disconnect_tail = network_close(tail);

    free(head->server_address);
    free(tail->server_address);


    free(head);
    free(tail);
    return disconnect_head + disconnect_tail;
}

/*Pedidos de escrita (put e delete) para o servidor head, de forma a serem propagados por toda a cadeia;
 */
int rtable_put_head(struct entry_t *entry)
{

    if (entry == NULL)
    {
        return -1; // Verificar se os argumentos são válidos.
    }

    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_PUT;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_ENTRY;

    EntryT entryT;

    entry_t__init(&entryT);
    entryT.key = entry->key;
    entryT.value.len = entry->value->datasize;
    entryT.value.data = entry->value->data;
    messageT.entry = &entryT;

    // Enviar a mensagem para o servidor e receba a resposta.

    MessageT *response = network_send_receive(head, &messageT);

    if (response == NULL)
    {
        return -1; // Erro na comunicação com o servidor.
    }

    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_PUT + 1)
    {
        // A operação foi realizada com sucesso.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return 0;
    }
    else
    {
        // Caso contrário, algo inesperado aconteceu.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
}

/*
Pedidos de leitura (get, size, getkeys e gettable) e pedido stats para o servidor tail.
*/
struct data_t *rtable_get_tail(char *key)
{
    if (key == NULL)
    {
        return NULL;
    }

    // Criar uma mensagem do tipo MESSAGE_T__OPCODE__OP_GET para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_GET;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_KEY;
    messageT.key = key;

    // Enviar a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(tail, &messageT);

    if (response == NULL)
    {
        return NULL; // Erro na comunicação com o servidor.
    }

    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_GET + 1)
    {
        // A operação foi realizada com sucesso.
        int size = response->value.len;
        char *data = strdup((char *)response->value.data);

        struct data_t *dataT = data_create(size, data);
        message_t__free_unpacked(response, NULL); // Liberar a mensagem de resposta.

        return dataT;
    }
    else
    {
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
}

/*Pedidos de escrita (put e delete) para o servidor head, de forma a serem propagados por toda a cadeia;
 */
int rtable_del_head(char *key)
{
    if (key == NULL)
    {
        return -1;
    }

    // Criar uma mensagem do tipo MESSAGE_T__OPCODE__OP_DEL para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_DEL;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_KEY;
    messageT.key = key;

    // Enviar a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(head, &messageT);

    if (response == NULL)
    {
        return -1; // Erro na comunicação com o servidor.
    }

    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_DEL + 1)
    {
        // A operação foi realizada com sucesso.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return 0;
    }
    else
    {

        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
}
/*
Pedidos de leitura (get, size, getkeys e gettable) e pedido stats para o servidor tail.
*/
int rtable_size_tail()
{

    // Criar uma mensagem do tipo MESSAGE_T__OPCODE__OP_SIZE para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_SIZE;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_NONE;

    // Enviar a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(tail, &messageT);

    if (response == NULL)
    {
        return -1; // Erro na comunicação com o servidor.
    }

    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_SIZE + 1)
    {
        // A operação foi realizada com sucesso.
        int size = response->result;
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return size;
    }
    else
    {

        // Caso contrário, algo inesperado aconteceu.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return -1;
    }
}

/*
Pedidos de leitura (get, size, getkeys e gettable) e pedido stats para o servidor tail.
*/
char **rtable_get_keys_tail()
{

    // Criar uma mensagem do tipo MESSAGE_T__OPCODE__OP_DEL para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_GETKEYS;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_NONE;

    // Enviar a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(tail, &messageT);

    if (response == NULL)
    {
        return NULL; // Erro na comunicação com o servidor.
    }

    // Resposta do servidor
    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_GETKEYS + 1)
    {
        // A operação foi realizada com sucesso.

        // Criar um array de strings para armazenar as chaves.
        int num_keys = response->n_keys;
        char **keys = (char **)malloc((num_keys + 1) * sizeof(char *));

        if (keys == NULL)
        {
            message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
            return NULL;
        }

        // Copiar as chaves da resposta para o array de strings.
        for (int i = 0; i < num_keys; i++)
        {
            keys[i] = strdup(response->keys[i]);
            if (keys[i] == NULL)
            {
                // Libertar a memória alocada até agora em caso de erro.
                for (int j = 0; j < i; j++)
                {
                    free(keys[j]);
                }
                free(keys);
                message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
                return NULL;
            }
        }

        keys[num_keys] = NULL; // Ùltimo elemento NULL.

        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return keys;
    }
    else
    {

        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
}

/* Liberta a memória alocada por rtable_get_keys().
 */
void rtable_free_keys(char **keys)
{
    if (keys != NULL)
    {
        // Libertar a memória associada a cada chave
        for (int i = 0; keys[i] != NULL; i++)
        {
            free(keys[i]);
        }
        // Libertar o próprio array de chaves
        free(keys);
    }
    else
    {
        perror("keys nao existem rtable_free_keys (client_stub)");
    }
}

/*
Pedidos de leitura (get, size, getkeys e gettable) e pedido stats para o servidor tail.
*/
struct entry_t **rtable_get_table_tail()
{

    // Cria uma mensagem do tipo MESSAGE_T__OPCODE__OP_DEL para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_GETTABLE;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_NONE;

    // Envia a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(tail, &messageT);

    if (response == NULL)
    {
        return NULL; // Erro na comunicação com o servidor.
    }

    // Verifica a resposta do servidor.
    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_GETTABLE + 1)
    {
        // A operação foi realizada com sucesso.

        // Cria um array de entry_t * para armazenar os dados da tabela.
        int num_entries = response->n_entries;
        struct entry_t **table = (struct entry_t **)malloc((num_entries + 1) * sizeof(struct entry_t *));

        if (table == NULL)
        {
            message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
            return NULL;
        }

        // Copia os dados da resposta para o array de entry_t *.
        for (int i = 0; i < num_entries; i++)
        {

            char *key = strdup((char *)response->entries[i]->key);
            int size = response->entries[i]->value.len;
            char *data = strdup((char *)response->entries[i]->value.data);
            struct data_t *dataT = data_create(size, data);

            table[i] = entry_create(key, dataT);

            if (table[i] == NULL)
            {

                // Libertar a memória alocada até agora em caso de erro.
                for (int j = 0; j < i; j++)
                {
                    entry_destroy(table[j]);
                }

                free(table);
                message_t__free_unpacked(response, NULL); // Liberta a mensagem de resposta.
                perror("Erro ao alocar memória client_stub (rtable_get_table)");

                return NULL;
            }
        }

        table[num_entries] = NULL; // Último elemento NULL.

        message_t__free_unpacked(response, NULL); // Liberta a mensagem de resposta.
        return table;
    }
    else
    {

        // Liberta a mensagem de resposta.
        message_t__free_unpacked(response, NULL);
        return NULL;
    }
}

/* Obtém as estatísticas do servidor. */
/*
Pedidos de leitura (get, size, getkeys e gettable) e pedido stats para o servidor tail.
*/
struct statistics_t *rtable_stats_tail()
{
    // Criar uma mensagem do tipo MESSAGE_T__OPCODE__OP_SIZE para enviar ao servidor.
    MessageT messageT;

    message_t__init(&messageT);
    messageT.opcode = MESSAGE_T__OPCODE__OP_STATS;
    messageT.c_type = MESSAGE_T__C_TYPE__CT_STATS;

    // Enviar a mensagem para o servidor e receba a resposta.
    MessageT *response = network_send_receive(tail, &messageT);

    if (response == NULL)
    {
        return NULL; // Erro na comunicação com o servidor.
    }

    if (response->opcode == MESSAGE_T__OPCODE__OP_ERROR)
    {
        // O servidor retornou um erro.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
    else if (response->opcode == MESSAGE_T__OPCODE__OP_STATS + 1)
    {
        // A operação foi realizada com sucesso.
        struct statistics_t *stats;
        stats = (struct statistics_t *)malloc(sizeof(struct statistics_t));
        stats->clientes = response->clientes;
        stats->tempo = response->tempo;
        stats->counter = response->counter;
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return stats;
    }
    else
    {

        // Caso contrário, algo inesperado aconteceu.
        message_t__free_unpacked(response, NULL); // Libertar a mensagem de resposta.
        return NULL;
    }
}

/* Liberta a memória alocada por rtable_get_table().
 */
void rtable_free_entries(struct entry_t **entries)
{

    if (entries != NULL)
    {

        // Percorremos o array de entry_t * e libertamos a memória de cada entrada.
        for (int i = 0; entries[i] != NULL; i++)
        {
            entry_destroy(entries[i]);
        }

        // Libertamos o próprio array de entrys
        free(entries);
    }
    else
    {
        perror("Erro client_stub: rtable_free_entries");
    }
}
/**
 * Watcher function for connection state change events
 */
void connection_watcher_client(zhandle_t *zzh, int type, int state, const char *path, void *context)
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

// Ligar ao zookeeper, iniciar a conexão.
void connect_client_to_zk(char *host_port)
{
    zh = zookeeper_init(host_port, connection_watcher_client, 2000, 0, NULL, 0);
    if (zh == NULL)
    {
        fprintf(stderr, "Error connecting to ZooKeeper server!\n");
        exit(EXIT_FAILURE);
    }
}

/*
 "Obter e fazer watch aos filhos de /chain".
 responsável por reagir a eventos que indicam mudanças nos filhos de "/chain"
 ao detectar uma mudança ,a função rtable_connect_zookeeper é chamada,
  que atualiza as conexões do cliente com os servidores head e tail da cadeia.
  */
void child_watcher_client(zhandle_t *wzh, int type, int state, const char *zpath, void *watcher_ctx)
{
    zoo_string *children_list = (zoo_string *)malloc(sizeof(zoo_string));
    if (state == ZOO_CONNECTED_STATE)
    {
        if (type == ZOO_CHILD_EVENT)
        {
            if (rtable_connect_zookeeper() != 0)
            {
                return;
            }
        }
        free(children_list);
    }
}
