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
  connect(dS, (struct sockaddr *)&aS, lgA);
  printf("Socket Connecté\n");

  char *buffer = (char *)malloc(MAX_LENGTH);
  while (1)
  {
    printf("Entrer un message de taille max %d: \n", MAX_LENGTH - 1);
    fgets(buffer, MAX_LENGTH, stdin);
    int taille = strlen(buffer) + 1;

    if (send(dS, &taille, sizeof(int), 0) < 0)
    {
      perror("Problème d'envoi de la taille");
      return 0;
    }
    printf("Taille Envoyée \n");

    if (send(dS, buffer, taille, 0) < 0)
    {
      perror("Problème d'envoi du message");
      return 0;
    }
    printf("Message Envoyé \n");

    int r;
    if (recv(dS, &r, sizeof(int), 0) < 0)
    {
      perror("Problème de réception du client");
      return 0;
    }
    printf("Réponse reçue : %d\n", r);
  }
  shutdown(dS, 2);
  printf("Fin du programme\n");
}