/**
 * ----------------------------------------------------------------------*
 * ----------------------GRUPO 18 - 2023/24-----------------------------*
 * ----------------------SISTEMAS DISTRIBUÍDOS---------------------------*
 * 
 *      PROJETO 4ª FASE
 * 
 *      FC56269 - Frederico Prazeres		
 *      FC56332 - Ricardo Sobral
 *      FC56353 - João Osório
 *
 * ----------------------------------------------------------------------*
 * ----------------------------------------------------------------------*
 */

4ª fase do projeto da unidade curricular Sistemas Distribuídos 2023/24 do 3º ano/1º semestre de LEI.
(EM LINUX)

Os ficheiros header (.h) estão incluidos na pasta include, e os ficheiros (.c) estão incluidos na pasta source.

Deve ser executado o makefile (correndo make na consola) e serão criados os ficheiros objeto (.o) no diretório object, o ficheiro libtable.a no diretório
lib,assim como os executáveis no diretório binary.

Os programas devem ser executados da seguinte forma (dentro do diretório binary):

        table-server <port> <n_lists> <zookeeperip><zookeeperport>  -   <port> é o número do porto TCP ao qual o servidor se deve associar (fazer bind).
                                                                        <n_lists> é o número de listas usado na criação da tabela no servidor.
                                                                        <zookeperip> é o ip em que o zookeeper está a correr
                                                                        <zookeeperport> é o porto em que o zookeeper está a correr

        
        table-client <zookeeperip> : <zookeeperport>  -   <server> é o endereço IP onde o zookeeper está a correr
                                                        <port> é o número do porto TCP onde o zookeeper se encontra

Correndo o comando "make clean" serão removidos os ficheiros (.o), o ficheiro libtable.a e ambos os executáveis.


