#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

// argv[1] = port

int main(int argc, char *argv[])
{

  printf("Début programme\n");

  int dS = socket(PF_INET, SOCK_STREAM, 0);
  if (dS < 0)
  {
    perror("Problème de création de socket serveur");
    return 0;
  }
  printf("Socket Créé\n");

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_addr.s_addr = INADDR_ANY;
  ad.sin_port = htons(atoi(argv[1]));
  bind(dS, (struct sockaddr *)&ad, sizeof(ad));
  printf("Socket Nommé\n");

  listen(dS, 7);
  printf("Mode écoute\n");

  struct sockaddr_in aC;
  socklen_t lg = sizeof(struct sockaddr_in);
  int dSC1 = accept(dS, (struct sockaddr *)&aC, &lg);
  if (dSC1 < 0)
  {
    perror("Problème lors de l'acceptation du client 1");
  }
  printf("Client 1 Connecté\n");

  int dSC2 = accept(dS, (struct sockaddr *)&aC, &lg);
  if (dSC2 < 0)
  {
    perror("Problème lors de l'acceptation du client 2");
  }
  printf("Client 2 Connecté\n");

  int taille = -1;
  while (1)
  {
    // Réception du client 1
    if (recv(dSC1, &taille, sizeof(int), 0) < 0 || taille < 0)
    {
      perror("Problème de réception de taille depuis le serveur");
      return 0;
    }
    printf("Taille reçue : %d\n", taille - 1);

    char *msg = malloc(sizeof(char) * taille);
    if (recv(dSC1, msg, taille, 0) < 0)
    {
      perror("Problème de réception du message depuis le serveur");
      return 0;
    }
    printf("Message reçu : %s\n", msg);

    // Envoi au client 2
    if (send(dSC2, &taille, sizeof(int), 0) < 0)
    {
      perror("Problème d'envoi de la taille depuis le serveur au client 2");
    }
    printf("Taille envoyée au client 2\n");

    if (send(dSC2, msg, sizeof(char) * taille, 0) < 0)
    {
      perror("Problème d'envoi depuis le serveur au client 2");
      return 0;
    }
    printf("Message Envoyé au client 2\n");
  }
  shutdown(dSC1, 2);
  shutdown(dSC2, 2);
  shutdown(dS, 2);
  printf("Fin du programme\n");
}
