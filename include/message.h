
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Function to read data from the socket until all data is received
int read_all(int sockfd, void *buffer, int length);

// Function to write data to the socket until all data is sent
int write_all(int sockfd,  void *buffer, int length);