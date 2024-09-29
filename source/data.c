
/**
 * ----------------------------------------------------------------------*
 * ----------------------GRUPO 18 - 2023/24-----------------------------*
 * ----------------------SISTEMAS DISTRIBUÍDOS---------------------------*
 * 
 *      PROJETO 1ª FASE
 * 
 *      FC56269 - Frederico Prazeres		
 *      FC56332 - Ricardo Sobral
 *      FC56353 - João Osório
 *
 * ----------------------------------------------------------------------*
 * ----------------------------------------------------------------------*
 */
#include "data.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


/* Função que cria um novo elemento de dados data_t e que inicializa 
 * os dados de acordo com os argumentos recebidos, sem necessidade de
 * reservar memória para os dados.	
 * Retorna a nova estrutura ou NULL em caso de erro.
 */
struct data_t *data_create(int size, void *data) {
    if (size <= 0 || data == NULL) {
        return NULL; // Caso o tamanho seja inválido ou a "data" seja nula
    }
    struct data_t *new_data = (struct data_t *)malloc(sizeof(struct data_t));
    if (new_data == NULL) {
        return NULL; // Caso a alocação de memória não seja bem-sucedida
    }
    new_data->datasize = size;
    new_data->data = data; // Aqui assume-se que a "data" já foi alocada anteriormente em outro local
    return new_data;
}


/* Função que elimina um bloco de dados, apontado pelo parâmetro data,
 * libertando toda a memória por ele ocupada.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int data_destroy(struct data_t *data) {
	//primeiro libertar memoria ocupada pelos atruibutos da stuct e só depois podemos libertar memoria ocupado pelo elemeto do tipo data_t
    if (data != NULL) {
        free(data->data); // Primeiro temos de libertar a memoria ocupada pela data 
        free(data);       // E só depois podemos libertar a memoria ocupada pela "data" do topo data_t 
        return 0;         // Eliminado com sucesso
    }
    return -1;            // O pointer do data é invalido
}


/* Função que duplica uma estrutura data_t, reservando a memória
 * necessária para a nova estrutura.
 * Retorna a nova estrutura ou NULL em caso de erro.
 */
struct data_t *data_dup(struct data_t *data) {
    if (data == NULL || data->datasize <= 0 || data->data == NULL) {
        return NULL; // Verifica se a estrutura de dados é válida
    }

    // Aloca memória para a nova estrutura data_t
    struct data_t *new_data = (struct data_t *)malloc(sizeof(struct data_t));
    if (new_data == NULL) {
        return NULL; // Caso a alocação de memória não seja bem-sucedida
    }

    // Inicializa o tamanho dos dados
    new_data->datasize = data->datasize;

    // memória para os dados e copia o conteúdo de 'data'
    new_data->data = malloc(data->datasize);
    if (new_data->data == NULL) {
        free(new_data); // Libera a estrutura data_t em caso de erro
        return NULL;
    }

    // Copia os dados da estrutura 'data' para a nova estrutura
    memcpy(new_data->data, data->data, data->datasize);

    return new_data;
}


/* Função que substitui o conteúdo de um elemento de dados data_t.
 * Deve assegurar que liberta o espaço ocupado pelo conteúdo antigo.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int data_replace(struct data_t *data, int new_size, void *new_data) {
    if (data == NULL || new_size <= 0 || new_data == NULL) {
        return -1; // Verifica se os argumentos são válidos
    }

    // Liberta a memória ocupada pelo conteúdo antigo
    free(data->data);

    // Atualiza o tamanho dos dados
    data->datasize = new_size;

    // Memória para os novos dados
    data->data = malloc(new_size);
    if (data->data == NULL) {
        return -1; // Caso a alocação de memória não seja bem-sucedida
    }

    // Copia os novos dados para a estrutura
    memcpy(data->data, new_data, new_size);

    return 0; // Substituição bem-sucedida
}

