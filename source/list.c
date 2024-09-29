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
#include "entry.h"
#include "list.h"
#include "list-private.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define _LIST_H /* Módulo list */

struct list_t; 

/* Função que cria e inicializa uma nova lista (estrutura list_t a
 * ser definida pelo grupo no ficheiro list-private.h).
 * Retorna a lista ou NULL em caso de erro.
 */
struct list_t *list_create() {

    struct list_t *new_list = (struct list_t *) malloc(sizeof(struct list_t)); //alocamos em memoria o espaço para a nova lista de entrys

    if (new_list == NULL) {
        return NULL; // Erro na alocação de memoria da lista
    }

    new_list->size = 0; // inicializamos com size 0...
    new_list->head = NULL; // e o pointer da head a NULL

    return new_list;
}


/* Função que elimina uma lista, libertando *toda* a memória utilizada
 * pela lista.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int list_destroy(struct list_t *list) {

    if (list == NULL) {
        return -1; // pointer da lista eh invalido
    }

    // Para eliminar a lista primeiro temos de eliminar todos os nós para não termos memory leaks
    while (list->head != NULL) {

        struct node_t *temp = list->head; //este será o nó que vamos eliminar
        list->head = list->head->next; // este será o proximo usado para correr iterar a lista

         
        if (temp->entry != NULL) { //caso o nó tenha uma entry
            entry_destroy(temp->entry); // Libertamos em memoria a estrutura entry com a função entry_destroy
        }

        free(temp); // Livertamos o nó
    }

    // Depois de eliminar todos os nós podemos libertar a lista
    free(list);

    return 0; //Sucesso! Lisa destroida 
}


/* Função que adiciona à lista a entry passada como argumento.
 * A entry é inserida de forma ordenada, tendo por base a comparação
 * de entries feita pela função entry_compare do módulo entry e
 * considerando que a entry menor deve ficar na cabeça da lista.
 * Se já existir uma entry igual (com a mesma chave), a entry
 * já existente na lista será substituída pela nova entry,
 * sendo libertada a memória ocupada pela entry antiga.
 * Retorna 0 se a entry ainda não existia, 1 se já existia e foi
 * substituída, ou -1 em caso de erro.
 */
int list_add(struct list_t *list, struct entry_t *entry) {

    //------------------------------------------------------------------------------//
    // Verificar se os pointers estão não estão nulos
    if (list == NULL || entry == NULL) {
        return -1; 
    }
    //------------------------------------------------------------------------------//

    //------------------------------------------------------------------------------//
    // Alocar memória para o nó que vai ser adicionado à lista (com a entry passada em argumento)
    struct node_t *node_to_insert = (struct node_t *) malloc(sizeof(struct node_t));
    if (node_to_insert == NULL) {
        return -1;
    }
    node_to_insert->entry = entry;
    node_to_insert->next = NULL;
    //------------------------------------------------------------------------------//


    //------------------------------------------------------------------------------//
    // Se a lista estiver vazia, adiciona-se o nó. Coloca-se a cabeça da lista à apontar para o novo nó.
    if (list->head == NULL) {
        list->head = node_to_insert;
        list->size = 1;
        return 0; // Entry added successfully
    }
    //------------------------------------------------------------------------------//

    /*
        Para iterarmos sobre a lista vamos ter um nó anterior e um nó atual, onde inicialmente o nó anterior vai estar a NULL, pois está atrás
        da cabeça da lista. É preciso ir guardando o estado de dois nós pois se o nó tiver que ser colocado no meio da lista, temos que alterar os ponteiros
        de dois nós, o anterior e o seguinte.
        Vamos iterar sobre a lista e comparando as chaves das entries usando o entry_compare() e andando para a frente com os nós.
        Se já existir uma entry com a mesma chave destrói-se a entry desse nó e substitui-se. 
    
    */
    struct node_t *anterior = NULL;
    struct node_t *atual = list->head;

    while (atual != NULL) { // Se o atual for nulo, já não estamos na lista

        int res = entry_compare(entry, atual->entry);  
        if (res == 0) { // Se res for 0, a entry tem a chave igual.
        
            entry_destroy(atual->entry); // Destrói-se a entry antiga
            atual->entry = entry;

            free(node_to_insert); // Este nó já não é preciso, apenas se substitui a entry

            return 1; // A entry já existia

        } else if (res < 0) { // Se res for <0 tem que se colocar o nó nessa posição, pois tem uma chave menor que a próxima.

            
            if (anterior == NULL) { // Se o nó anterior for nulo, estamos no ínicio da lista. O nó a inserir passará a ser a cabeça da lista.
                
                node_to_insert->next = list->head;
                list->head = node_to_insert;

            } else { // Se não, o next do nó a inserir é o atual e o próximo do anterior é o nó a inserir
        
                node_to_insert->next = atual;
                anterior->next = node_to_insert;
            }
            list->size++;
            return 0; // A entry não existia
        }
        anterior = atual;
        atual = atual->next;
    }

    // Sai se do ciclo, o nó tem que ir para o fim da lista
    anterior->next = node_to_insert;
    list->size++;
    return 0; // A entry não existia
}


/* Função que elimina da lista a entry com a chave key, libertando a
 * memória ocupada pela entry.
 * Retorna 0 se encontrou e removeu a entry, 1 se não encontrou a entry,
 * ou -1 em caso de erro.
 */
int list_remove(struct list_t *list, char *key) {
    if (list == NULL || key == NULL) {
        return -1; // Erro: Ponteiros inválidos
    }

    struct node_t *prev = NULL;
    struct node_t *current = list->head;

    while (current != NULL) {
        if (strcmp(current->entry->key, key) == 0) {
            // Encontrou a entrada com a chave desejada
            if (prev == NULL) {
                // A entrada está na cabeça da lista
                list->head = current->next;
            } else {
                // A entrada não está na cabeça, atualiza o ponteiro prev->next
                prev->next = current->next;
            }

            // Liberta-se a memória ocupada pela entrada
            entry_destroy(current->entry);
            free(current);

            list->size--; // Decrementa o tamanho da lista
            return 0; // Entrada encontrada e removida com sucesso
        }

        prev = current;
        current = current->next;
    }

    return 1; // Entrada não encontrada na lista
}


/* Função que obtém da lista a entry com a chave key.
 * Retorna a referência da entry na lista ou NULL se não encontrar a
 * entry ou em caso de erro.
*/
struct entry_t *list_get(struct list_t *list, char *key){

    if (list == NULL || key == NULL) { // Verificar se os pointers não estão nulos
        return NULL; 
    }

    if(list->head==NULL){ // A lista está vazia
        return NULL;
    }

    struct node_t *node = list->head;

    while(node!=NULL){

        if(strcmp(key,node->entry->key)==0){ // Encontrou-se a entry com a chave key na lista
            return node->entry;
        }

        node = node->next;
    }
    // Se sai do ciclo, não encontrou, retorna-se NULL

    return NULL;

}

/* Função que conta o número de entries na lista passada como argumento.
 * Retorna o tamanho da lista ou -1 em caso de erro.
 */
int list_size(struct list_t *list){

    if (list == NULL) { // Verificar se os pointer estao nulos
        return -1; 
    }else{
        return list->size;
    }

}

/* Função que constrói um array de char* com a cópia de todas as keys na 
 * lista, colocando o último elemento do array com o valor NULL e
 * reservando toda a memória necessária.
 * Retorna o array de strings ou NULL em caso de erro.
 */
char **list_get_keys(struct list_t *list) {
    if (list == NULL || list->size == 0) {
        return NULL; // Verifique se a lista não é nula e não está vazia
    }

    char **keys = (char **)malloc((list->size + 1) * sizeof(char *));
    if (keys == NULL) {
        return NULL; // Erro na alocação de memória para o array de keys
    }

    struct node_t *current = list->head;
    int index = 0;

    while (current != NULL) {
        // Obtem-se a key da entrada atual e aloque memória para armazená-la no array de keys
        keys[index] = strdup(current->entry->key);
        if (keys[index] == NULL) {
            // Erro na alocação de memória para uma das keys, liberta-se a memória alocada anteriormente
            for (int i = 0; i < index; i++) {
                free(keys[i]);
            }
            free(keys);
            return NULL;
        }
        current = current->next;
        index++;
    }

    keys[index] = NULL; // Coloca-se o último elemento como NULL
    return keys;
}

/* Função que liberta a memória ocupada pelo array de keys obtido pela 
 * função list_get_keys.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int list_free_keys(char **keys) {
    if (keys == NULL) {
        return -1; // Verifique se o array de keys não é nulo
    }

    // Liberta-se a memória alocada para cada key no array
    for (int i = 0; keys[i] != NULL; i++) {
        free(keys[i]);
    }

    free(keys); // Liberta-se a memória alocada para o próprio array de keys
    return 0;
}

