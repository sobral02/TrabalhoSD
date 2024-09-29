#include "message.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

// Função que lê da socket até ter sido tudo recebido
int read_all(int sockfd, void *buffer, int length) {

    int bufsize = length;

    while(length>0) {

        int res = read(sockfd, buffer, length);

        if(res < 1) {
            if(errno==EINTR) continue;
            return -1;
        }
        buffer += res;
        length -= res;
    }
    return bufsize;
}

// Função que escreve na socket até ter sido tudo enviado
int write_all(int sockfd, void *buf, int len) {
     int bufsize = len;

    while(len>0) {

        int res = write(sockfd, buf, len);
        if(res < 0) {
            if(errno==EINTR) continue;
            return -1;
        }
        
        if(res == 0) return res;
        buf += res;
        len -= res;
    }
    return bufsize;
}