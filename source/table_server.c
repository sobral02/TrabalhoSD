#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "network_server.h"
#include "table_skel.h"
#include "table_skel-private.h"

int main(int argc, char **argv){
    if(argc != 4){
        printf("Deve introduzir: table-server <port> <n_lists> <IPzookeeper:Portozookeeper>\n");
        return -1;
    }

    int sockfd = network_server_init(atoi(argv[1]));

    if (sockfd == -1) {
        perror("Erro ao iniciar o servidor de rede");
        return -1;
    }

    struct table_t *table = table_skel_init(atoi(argv[2]));

    if (table == NULL) {
        perror("Erro ao inicializar a tabela");
        network_server_close(sockfd);
        return -1;
    } else {
        printf("Tabela iniciada\n");
    }

    //conectar o zookeeper com os argumentos corretos, saltando o número de listas
    if(init_zookeeper_connection(argv[1], argv[3]) == -1){
        return -1;
    }

    int result = network_main_loop(sockfd, table);

    network_server_close(sockfd);

    if (result == -1) {
        perror("Erro na função network_main_loop");
    }

    return result;
}
