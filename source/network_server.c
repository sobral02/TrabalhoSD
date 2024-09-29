#include "table_skel.h"
#include "network_server.h"
#include "message.h"
#include "table.h"
#include "misc.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdmessage.pb-c.h"
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>


int sockfd;
struct sockaddr client;
struct sigaction sa;
socklen_t size_client;
struct table_t *tabela;


void ctrlC() {
    table_skel_destroy(tabela);
    network_server_close(sockfd);
    exit(1);
} 

/* Função para preparar um socket de receção de pedidos de ligação
 * num determinado porto.
 * Retorna o descritor do socket ou -1 em caso de erro.
 */
int network_server_init(short port){ 

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("Erro ao criar socket");
        return -1;
    }
    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port); 
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt = 1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char*) &opt,sizeof(opt)) < 0){
        printf("setsockopt failed\n");
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0){
        perror("Erro ao fazer bind\n");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 0) < 0){
        perror("Erro ao executar listen\n");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void *client_handler(void *conn_socket) {
    int connsockfd = *((int *)conn_socket);

    MessageT *message;
    while ((message = network_receive(connsockfd)) != NULL) {

        if(message == NULL || invoke(message, tabela) == -1){

            perror("Network Server invoke 80");


        }
        
        if(network_send(connsockfd, message) == -1){
            perror("Network Server send 84");
        }

        message_t__free_unpacked(message, NULL);
    }
    
    decrease_client();
    //printf("Client Disconnected\n");
    close(connsockfd);  /* Close the connection */
    pthread_exit(NULL);
}


/* A função network_main_loop() deve:
 * - Aceitar uma conexão de um cliente;
 * - Receber uma mensagem usando a função network_receive;
 * - Entregar a mensagem de-serializada ao skeleton para ser processada
     na tabela table;
 * - Esperar a resposta do skeleton;
 * - Enviar a resposta ao cliente usando a função network_send.
 * A função não deve retornar, a menos que ocorra algum erro. Nesse
 * caso retorna -1.
 */
int network_main_loop(int listening_socket, struct table_t *table){
    tabela = table;
    sa.sa_handler = ctrlC;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask); 

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("main:");
        exit(0);
    }

     while (1) {
        int connsockfd = accept(listening_socket, NULL, NULL);
        if (connsockfd != -1) {
            //printf("Client Connected\n");
            increase_client();


            pthread_t client_thread;
            if (pthread_create(&client_thread, NULL, client_handler, (void *)&connsockfd) != 0) {
                perror("pthread_create");
                close(connsockfd);
            }else{
                pthread_detach(client_thread);
            }
        } else {
            perror("accept");
        }
    }
    return 0;

}

/* A função network_receive() deve:
 * - Ler os bytes da rede, a partir do client_socket indicado;
 * - De-serializar estes bytes e construir a mensagem com o pedido,
 *   reservando a memória necessária para a estrutura MessageT.
 * Retorna a mensagem com o pedido ou NULL em caso de erro.
 */
MessageT *network_receive(int client_socket) {

    int nbytes = 0;
    short size;
    if (client_socket < 0) {
        return NULL;
    }

    if((nbytes = read_all(client_socket, &size, sizeof(short))) == -1) {
        //perror("Couldnt receive data from the client");
        return NULL;
    }

    short host_size = ntohs(size);

    unsigned char* buffer = malloc(host_size);

    if((nbytes = read_all(client_socket, buffer, host_size)) == -1) {
        //perror("Couldnt receive data from the client");
        return NULL;
    }

    // Deserializa-se a mensagem do cliente
    MessageT *msg = message_t__unpack(NULL, nbytes, (const unsigned char *) buffer);
    if (msg == NULL) {
        perror("Erro ao dar unpack à mensagem (network_server)");
        return NULL;
    }

    free(buffer);

    return msg;

}

/* A função network_send() deve:
 * - Serializar a mensagem de resposta contida em msg;
 * - Enviar a mensagem serializada, através do client_socket.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int network_send(int client_socket, MessageT *msg) {

    if (client_socket < 0 || msg == NULL) {
        return -1;
    }

    size_t length = message_t__get_packed_size((const struct _MessageT*) msg);
    unsigned char *msg_content_buf = malloc(length);

    if (msg_content_buf == NULL) {
        return -1;
    }

    int nbytes;
    //Serializa a mensagem contida em msg;
    size_t packedLength = message_t__pack(msg, msg_content_buf);

    short net_byte_order_message_size = htons(length);


    //Envia o tamanho da mensagem para o cliente;
    if((nbytes = write_all(client_socket, &net_byte_order_message_size, sizeof(short))) == -1) {
        perror("Couldnt send data size to the client");
        free(msg_content_buf);
        return -1;
    }

    //Envia a mensagem serializada para o cliente
    if((nbytes = write_all(client_socket, msg_content_buf, packedLength)) == -1) {
        perror("Couldnt send data to the client");
        free(msg_content_buf);
        return -1;
    }

    free(msg_content_buf);


    return 0;
}

/* Liberta os recursos alocados por network_server_init(), nomeadamente
 * fechando o socket passado como argumento.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int network_server_close(int socket){
    return close(socket);
}

