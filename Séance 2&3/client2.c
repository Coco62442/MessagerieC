#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LENGTH 10

// argv[1] = adresse ip
// argv[2] = port

int main(int argc, char *argv[])
{

  printf("Début programme\n");
  int dS = socket(PF_INET, SOCK_STREAM, 0);
  if (dS == -1)
  {
    perror("Problème de création de socket client");
    return 0;
  }
  printf("Socket Créé\n");

  struct sockaddr_in aS;
  aS.sin_family = AF_INET;
  inet_pton(AF_INET, argv[1], &(aS.sin_addr));
  aS.sin_port = htons(atoi(argv[2]));
  socklen_t lgA = sizeof(struct sockaddr_in);
  if (connect(dS, (struct sockaddr *)&aS, lgA) < 0)
  {
    perror("Problème de connexion au serveur");
  }
  printf("Socket Connecté\n");

  char *buffer = (char *)malloc(MAX_LENGTH);
  int taille = -1;
  while (1)
  {
    if (recv(dS, &taille, sizeof(int), 0) < 0 || taille < 0)
    {
      perror("Problème de réception de la taille du message client 2");
      return 0;
    }
    printf("Taille reçue : %d\n", taille - 1);

    char *buffer = malloc(sizeof(char) * taille);
    if (recv(dS, buffer, sizeof(char) * taille, 0) < 0)
    {
      perror("Problème de réception du message client 2");
    }
    printf("Réponse reçue : %s\n", buffer);
  }
  free(buffer);
  shutdown(dS, 2);
  printf("Fin du programme\n");
}