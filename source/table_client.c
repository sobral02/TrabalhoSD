#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <client_stub-private.h>
#include <client_stub.h>
#include <entry.h>
#include <data.h>
#include <network_client.h>
#include <signal.h>
#include "misc.h"
#include "stats.h"

int main(int argc, char *argv[]) {
    
    signal(SIGINT, sig_handler);
    signal(SIGSEGV, sig_handler);
    signal(SIGTSTP, sig_handler);		
    signal(SIGABRT, sig_handler);

    if (argc != 2) { // No caso do cliente não ser bem inicializado
        fprintf(stderr, "Uso correto para table_client: %s <zookeeperip>:<zookeeperport>\n", argv[0]);
        return 1;
    }
    connect_client_to_zk(argv[1]);
   
   // char* server_info = argv[1];
    
    
    if (rtable_connect_zookeeper() == -1) {
        fprintf(stderr, "Erro ao conectar com o servidor\n");
        return 1;
    }

    printCommands();

    // Loop de comandos
    char command[100];
    char *token;
    char *key, *data;
    while (1) {
        printf("Insira um comando: \n");

        if (fgets(command, sizeof(command), stdin) == NULL) {
            break; 
        }

        // Remove o caractere de nova linha do comando
        command[strcspn(command, "\n")] = '\0';

        token = strtok(command, " ");
        if (token == NULL) {
            printf("Comando inválido. Tente novamente.\n");
            continue;
        }
        
        if (strcmp(token, "put") == 0) {
            key = strtok(NULL, " ");
            data = strtok(NULL, "\n");
            if (key != NULL && data != NULL) {
                struct data_t *data_struct = data_create(strlen(data) + 1, data);
                if (data_struct != NULL) {
                    struct entry_t *entry = entry_create(key, data_struct);
                    if (entry != NULL) {
                        if (rtable_put_head(entry) == 0) {
                            printf("Operação bem-sucedida.\n");
                        } else {
                            printf("Erro na operação put.\n");
                        }
                        free(entry->value);
                        free(entry);
                    } else {
                        data_destroy(data_struct);
                        printf("Erro ao criar a entrada.\n");
                    }
                } else {
                    printf("Erro ao criar a estrutura de dados.\n");
                }
            } else {
                printf("Comando put incorreto. Use: put <key> <data>\n");
            }
        } else if (strcmp(token, "get") == 0) {
            key = strtok(NULL, "");
            if (key != NULL) {
                struct data_t *result = rtable_get_tail(key);
                if (result != NULL) {
                    printf("Valor encontrado: %s\n", (char *)result->data);
                    data_destroy(result);
                } else {
                    printf("Chave não encontrada.\n");
                }
            } else {
                printf("Comando get incorreto. Use: get <key>\n");
            }
        } else if (strcmp(token, "del") == 0) {
            key = strtok(NULL, "");
            if (key != NULL) {
                if (rtable_del_head(key) == 0) {
                    printf("Remoção bem-sucedida.\n");
                } else {
                    printf("Erro na remoção.\n");
                }
            } else {
                printf("Comando del incorreto. Use: del <key>\n");
            }
        } else if (strcmp(token, "size") == 0) {
            int size = rtable_size_tail();
            if (size >= 0) {
                printf("Tamanho da tabela: %d\n", size);
            } else {
                printf("Erro ao obter o tamanho da tabela.\n");
            }
        } else if (strcmp(token, "getkeys") == 0) {
            char **keys = rtable_get_keys_tail();
            if (keys != NULL) {
                printf("Chaves na tabela:\n");
                for (int i = 0; keys[i] != NULL; i++) {
                    printf("%s\n", keys[i]);
                    free(keys[i]);
                }
                free(keys);
            } else {
                printf("Erro ao obter as chaves da tabela.\n");
            }
        } else if (strcmp(token, "gettable") == 0) {
            struct entry_t **entries = rtable_get_table_tail();
            if (entries != NULL) {
                printf("Conteúdo da tabela:\n");
                for (int i = 0; entries[i] != NULL; i++) {
                    printf("Key: %s, Valor: %s\n", entries[i]->key, (char *)entries[i]->value->data);
                    free(entries[i]->key);
                    data_destroy(entries[i]->value);
                    free(entries[i]);
                }
                free(entries);
            } else {
                printf("Erro ao obter o conteúdo da tabela.\n");
            }
        } else if (strcmp(token, "stats") == 0) {
            struct statistics_t *stats = rtable_stats_tail();
            if (stats != NULL) {
                printf("Estatísticas do servidor:\n");
                printf(" Número de Operações: %d\n Tempo total de operações (em microsegundos): %ld\n Clientes conectados: %d\n", stats->counter, stats->tempo, stats->clientes);
                free(stats);
            } else {
                printf("Erro ao obter as estatísticas da tabela.\n");
            }
        }else if (strcmp(token, "quit") == 0) {
            if (rtable_disconnect_head_tail() == -1) {
                fprintf(stderr, "Erro ao desconectar do servidor\n");
            }
            break;
        } else {
            printf("Comando não reconhecido. Tente novamente.\n");
        }
    }

    return 0;
}

void sig_handler(int signum) {
    rtable_disconnect_head_tail();
    exit(-1);
}

void printCommands() {
    printf("Comandos disponíveis: \n\n");
    printf("put <key> <data>    - Esta função permite ao utilizador colocar uma entrada na tabela com chave key e valor data\n\n");
    printf("get <key>           - Devolve a entrada da tabela identificada pela chave(key)\n\n");
    printf("del <key>           - Elimina uma entrada da tabela identificada pela chave(key)\n\n");
    printf("size                - Devolve o numero de entradas na tabela\n\n");
    printf("getkeys             - Devolve todas as chaves (keys) das entradas da tabela\n\n");
    printf("gettable            - Devolve a chave e os dados de cada entrada na tabela\n\n");
    printf("quit                - Termina o programa\n\n");
}
