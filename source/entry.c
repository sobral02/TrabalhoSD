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
#include "entry.h"
#include <stdlib.h>
#include <string.h>


/* Função que cria uma entry, reservando a memória necessária e
 * inicializando-a com a string e o bloco de dados de entrada.
 * Retorna a nova entry ou NULL em caso de erro.
 */
struct entry_t *entry_create(char *key, struct data_t *data) {
    if (key == NULL || data == NULL) return NULL;
	
    struct entry_t* entry = malloc(sizeof(struct entry_t));
    
    if (entry == NULL) return NULL;
    
    entry->key = key;
    entry->value = data;
    
    return entry;
}


/* Função que elimina uma entry, libertando a memória por ela ocupada.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int entry_destroy(struct entry_t *entry) {
    if (entry == NULL || entry->key == NULL || entry->value == NULL) return -1;
    
    data_destroy(entry->value);
    free(entry->key);
    free(entry);
    
    return 0;
}


/* Função que duplica uma entry, reservando a memória necessária para a
 * nova estrutura.
 * Retorna a nova entry ou NULL em caso de erro.
 */
struct entry_t *entry_dup(struct entry_t *entry) {

    if (entry == NULL) {
    return NULL;
    }

    struct entry_t *new_entry = (struct entry_t *)malloc(sizeof(struct entry_t)); //alocamos em memoria o epaço para a nova entrada

    if (new_entry == NULL) {
        return NULL; 
    }
	
    new_entry->key = strdup(entry->key); // a string da key é duplicada para nova entrada

    if (new_entry->key == NULL) {
        free(new_entry); // libertamos a memoria ocupada pela entry pq ocorreu um erro a duplicar a string key
        return NULL;
    }

    new_entry->value = data_dup(entry->value); // Duplica-se tbm o valor da data

    if (new_entry->value == NULL) {//caso haja um erro...
        free(new_entry->key); // temos de libertar a memoria ocupada pela key que já terá o valor da key da entrada original
        free(new_entry); // e só assim já podemos libertar a memoria
        return NULL;
    }

    return new_entry; //duplicação de sucesso
}


/* Função que substitui o conteúdo de uma entry, usando a nova chave e
 * o novo valor passados como argumentos, e eliminando a memória ocupada
 * pelos conteúdos antigos da mesma.
 * Retorna 0 (OK) ou -1 em caso de erro.
 */
int entry_replace(struct entry_t *entry, char *new_key, struct data_t *new_value) {

    if (entry == NULL || new_key == NULL || new_value == NULL) return -1;
    data_destroy(entry->value);
    free(entry->key);
    entry->key = new_key;
    entry->value = new_value;
    return 0;
}


/* Função que compara duas entries e retorna a ordem das mesmas, sendo esta
 * ordem definida pela ordem das suas chaves.
 * Retorna 0 se as chaves forem iguais, -1 se entry1 < entry2,
 * 1 se entry1 > entry2 ou -2 em caso de erro.
 */
int entry_compare(struct entry_t *entry1, struct entry_t *entry2) {

    if (entry1 != NULL && entry2 != NULL) { //temos de verificar se as entrys existem...
        int a = strcmp(entry1->key, entry2->key); //como o strcmp não retrorna so 1 , 0 -1 
                                                //temos de usar uma variavel para ajudar
		if (a > 0 ){
			return 1; //entry1 maior
		}else if (a < 0){
			return -1; //entry2 maior
		}else{
			return 0; //são iguais
		}
		
    }
    return -2; //Caso uma ou as duas entrys não existam
}
