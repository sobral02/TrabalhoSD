#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include "message.h"
#include "network_client.h"
#include "client_stub.h"
#include "client_stub-private.h"

#include "sdmessage.pb-c.h"

/* Esta função deve:
 * - Obter o endereço do servidor (struct sockaddr_in) com base na
 *   informação guardada na estrutura rtable;
 * - Estabelecer a ligação com o servidor;
 * - Guardar toda a informação necessária (e.g., descritor do socket)
 *   na estrutura rtable;
 * - Retornar 0 (OK) ou -1 (erro).
 */

int network_connect(struct rtable_t *rtable) {

    if (rtable == NULL || rtable->server_address == NULL) { // Verifica-se se rtable está nula
        return -1;
    }

    int sockfd;
    struct sockaddr_in server;

    // Cria socket TCP
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        return -1;
    }

    // Preenche estrutura server para estabelecer conexao
    server.sin_family = AF_INET;
    server.sin_port = htons(rtable->server_port);
    if (inet_pton(AF_INET, rtable->server_address, &server.sin_addr) < 1) {
        printf("Erro ao converter IP\n");
        close(sockfd);
        return -1;
    }

    // Estabelece conexao com o servidor definido em server
    if (connect(sockfd,(struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Erro ao conectar-se ao servidor");
        close(sockfd);
        return -1;
    }

    rtable->sockfd = sockfd;
    
    return 0;

    
}


/* Esta função deve:
 * - Obter o descritor da ligação (socket) da estrutura rtable_t;
 * - Serializar a mensagem contida em msg;
 * - Enviar a mensagem serializada para o servidor;
 * - Esperar a resposta do servidor;
 * - De-serializar a mensagem de resposta;
 * - Tratar de forma apropriada erros de comunicação;
 * - Retornar a mensagem de-serializada ou NULL em caso de erro.
 */

MessageT *network_send_receive(struct rtable_t *rtable, MessageT *msg) {
    
    if (rtable == NULL || msg == NULL) { // Verifica se os pointers são válidos
        return NULL; 
    }

    // File descriptor de rtable
    int sockfd = rtable->sockfd;

    

    /*-----------------------------SEND-----------------------------*/

    
    size_t length = message_t__get_packed_size((const struct _MessageT*) msg);
    unsigned char *msg_content_buf = malloc(length);

    if (msg_content_buf == NULL) {
        rtable_disconnect(rtable);
        return NULL;
    }

    int nbytes;
    //Serializa a mensagem contida em msg;
    size_t packedLength = message_t__pack(msg, msg_content_buf);

    short net_byte_order_message_size = htons(length);


    //Envia a mensagem serializada para o servidor;
    if((nbytes = write_all(sockfd, &net_byte_order_message_size, sizeof(short))) == -1) {
        perror("Couldnt send data to the server");
        rtable_disconnect(rtable);
        free(msg_content_buf);
        return NULL;
    }


    if((nbytes = write_all(sockfd, msg_content_buf, packedLength)) == -1) {
        perror("Couldnt send data to the server");
        rtable_disconnect(rtable);
        free(msg_content_buf);
        return NULL;
    }

    free(msg_content_buf);

    /*----------------------------------------------------------*/


    /*-----------------------------RECEIVE-----------------------------*/

  

    if((nbytes = read_all(sockfd, &net_byte_order_message_size, sizeof(short))) == -1) {
        perror("Couldnt receive data from the server");
        rtable_disconnect(rtable);
        return NULL;
    }

    short host_size = ntohs(net_byte_order_message_size);

    unsigned char* buffer = malloc(host_size);

    if((nbytes = read_all(sockfd, buffer, host_size)) == -1) {
        perror("Couldnt receive data from the server");
        rtable_disconnect(rtable);
        return NULL;
    }

    // Deserializa-se a mensagem do servidor
    MessageT *response_msg = message_t__unpack(NULL, nbytes, (const unsigned char *) buffer);
    if (response_msg == NULL) {
        perror("Erro ao dar unpack à mensagem (network_client)");
        rtable_disconnect(rtable);
        return NULL;
    }

    free(buffer);

    return response_msg;
}

int network_close(struct rtable_t *rtable) {
    if (rtable == NULL) { // Verificação de pointers
        return -1;  
    }

    if (close(rtable->sockfd) == 0) {
        return 0;
    }else{
        perror("Erro ao desconectar do servidor (network_close)");
        return -1;
    }

}

