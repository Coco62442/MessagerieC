#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char *msg = (char *)malloc(sizeof(char) * 100);
    strcpy(msg, "/mp test");
    printf("%s\n", msg);
    char *strToken = strtok(msg, " ");
    printf("%s\n", strToken);
    strToken = strtok(NULL, " ");
    printf("%s\n", strToken);
    strToken = strtok(NULL, " ");
    printf("%d\n", strToken == NULL);
    char *pseudo = (char *)malloc(sizeof(char) * 100);
    printf("%d\n", pseudo == "\0");
    printf("%s\n", pseudo);
}