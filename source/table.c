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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "data.h"
#include "entry.h"
#include "list.h"
#include "list-private.h" 
#include "table-private.h"
#include "table.h"

/* Função para criar e inicializar uma nova tabela hash, com n
 * linhas (n = módulo da função hash).
 * Retorna a tabela ou NULL em caso de erro.
 */
struct table_t *table_create(int n) {
    if (n <= 0) {
        return NULL;
    }

    struct table_t *table = (struct table_t *)malloc(sizeof(struct table_t));
    if (table == NULL) {
        return NULL;
    }

    table->size = n;
    table->lists = (struct list_t **) malloc(n * sizeof(struct list_t *));
    if (table->lists == NULL) {
        free(table);
        return NULL; 
    }

    //Inicializa todas as listas na tabela
    for (int i = 0; i < n; i++) {
        table->lists[i] = list_create();
        if (table->lists[i] == NULL) {
            //Se houver erro, libertar a memória antes de return null
            for (int j = 0; j < i; j++) {
                list_destroy(table->lists[j]);
            }
            free(table->lists);
            free(table);
            return NULL;
        }
    }

    return table;
}

/* Função que elimina uma tabela, libertando *toda* a memória utilizada
 * pela tabela.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int table_destroy(struct table_t *table) {
    if (table == NULL) {
        return -1; 
    }

    for (int i = 0; i < table->size; i++) { // Liberta todas as listas na tabela
        list_destroy(table->lists[i]);
    }

    free(table->lists); //libertação de memória
    free(table);
    return 0;
}

/* Função para adicionar um par chave-valor à tabela. Os dados de entrada
 * desta função deverão ser copiados, ou seja, a função vai criar uma nova
 * entry com *CÓPIAS* da key (string) e dos dados. Se a key já existir na
 * tabela, a função tem de substituir a entry existente na tabela pela
 * nova, fazendo a necessária gestão da memória.
 * Retorna 0 (ok) ou -1 em caso de erro.
 */
int table_put(struct table_t *table, char *key, struct data_t *value){

    if (table == NULL || key == NULL || value == NULL) { // Verifica se os pointers são válidos
        return -1; 
    }

    int hash = hash_code(key, table->size);
    struct list_t* lista = table->lists[hash];

    struct entry_t* entry = list_get(lista,key);

    if(entry==NULL){ //Entry não existe na lista (e na tabela)

        struct data_t* new_data = data_dup(value);
        if(new_data==NULL){ //Erro ao duplicar os dados
            return -1;
        }

        char* new_key = strdup(key);
        if (new_key == NULL) { //Erro ao duplicar a chave
            free(new_data);
            return -1; 
        }

        struct entry_t* new_entry = entry_create(new_key, new_data);
        if(new_entry==NULL){ //Erro na alocação de mmoria para a nova entry, destrói-se os dados duplicados
            data_destroy(new_data);
            free(new_key);
            return -1;
        }

        int res = list_add(lista, new_entry);
        if(res==-1){ //Erro ao adicionar a nova entry à lista, liberta-se a entry duplicada
            entry_destroy(new_entry);
            return -1;
        }

        return 0;

    }else{ // Entry já existe, basta substituir os dados (e libertar os anteriores)

        struct data_t* new_data = data_dup(value);
        if(new_data==NULL){ //Erro ao duplicar os dados
            return -1;
        }

        data_destroy(entry->value);
        entry->value = new_data;


        return 0;


    }


}

/* Função que procura na tabela uma entry com a chave key. 
 * Retorna uma *CÓPIA* dos dados (estrutura data_t) nessa entry ou 
 * NULL se não encontrar a entry ou em caso de erro.
 */
struct data_t *table_get(struct table_t *table, char *key){

    if (table == NULL || key == NULL ) {
        return NULL; 
    }

    int hash = hash_code(key,table->size);
    struct list_t* lista = table->lists[hash];

    struct entry_t* entry = list_get(lista,key);

    if(entry==NULL){
        return NULL;
    }else{ // Entry existe, devolve-se os dados duplicados
        
        return data_dup(entry->value);

    }

}

/* Função que remove da lista a entry com a chave key, libertando a
 * memória ocupada pela entry.
 * Retorna 0 se encontrou e removeu a entry, 1 se não encontrou a entry,
 * ou -1 em caso de erro.
 */
int table_remove(struct table_t *table, char *key){

    if (table == NULL || key == NULL ) { // Verifica se os pointers são válidos
        return -1; 
    }

    int hash = hash_code(key,table->size);
    struct list_t* lista = table->lists[hash];

    return list_remove(lista, key); // list_remove procura e retorna 0, 1 e -1 consoante o resultado

}

/* Função que conta o número de entries na tabela passada como argumento.
 * Retorna o tamanho da tabela ou -1 em caso de erro.
 */
int table_size(struct table_t *table) {
    if (table == NULL) {
        return -1;
    }

    int total_entries = 0;

    //Percorrer todas as listas na tabela e somar o número de entries
    for (int i = 0; i < table->size; i++) {
        struct list_t *list = table->lists[i];
        if (list != NULL) {
            total_entries += list_size(list);
        }
    }

    return total_entries;
}

/* Função que constrói um array de char* com a cópia de todas as keys na 
 * tabela, colocando o último elemento do array com o valor NULL e
 * reservando toda a memória necessária.
 * Retorna o array de strings ou NULL em caso de erro.
 */
char **table_get_keys(struct table_t *table){
	if (table == NULL){
		return NULL;
	}
    char **keys = malloc((table_size(table) + 1) * sizeof(char*));
	int i, j, index = 0;
	for (i = 0; i < table->size; i++){
    	char** list_keys = list_get_keys(table->lists[i]);
		for(j = 0; j < list_size(table->lists[i]); j++){
			int length = strlen(list_keys[j]) + 1;
			keys[index] = (char*) malloc(length);
			if (keys[index] == NULL){
				return NULL;
			}
            memcpy(keys[index], list_keys[j], length);
            index++;
		}
		list_free_keys(list_keys);
	}
	keys[index] = NULL;
	return keys;
}

/* Função que liberta a memória ocupada pelo array de keys obtido pela 
 * função table_get_keys.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int table_free_keys(char **keys){


    if(keys==NULL){ // Verificar se o pointer é válido
        return -1;
    }

    int counter = 0;

    while(keys[counter]!=NULL){

        free(keys[counter]);

        counter++;

    }

    free(keys);
    return 0;

}


/* Função que calcula o índice da lista a partir da chave
 */
int hash_code(char *key, int n){

    if (key == NULL || n <= 0) { // Verifica-se se os pointers estão válidos
        return 0; 
    }

    int soma = 0;
    int len = strlen(key);

    for (int i = 0; i < len; i++) {
        soma += key[i]; // Soma os valores ASCII dos caracteres.
    }


    return soma % n; // Hash

}

