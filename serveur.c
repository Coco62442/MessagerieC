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
  if (dS == -1)
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
  int dSC = accept(dS, (struct sockaddr *)&aC, &lg);
  printf("Client Connecté\n");

  int taille = -1;
  int r = 10;
  while (1)
  {
    if (recv(dSC, &taille, sizeof(int), 0) < 0 || taille == -1)
    {
      perror("Problème de réception de taille depuis le serveur");
      return 0;
    }
    printf("Taille reçue : %d\n", taille);

    char msg[taille];
    if (recv(dSC, msg, taille, 0) < 0)
    {
      perror("Problème de réception du message depuis le serveur");
      return 0;
    }
    printf("Message reçu : %s\n", msg);

    if (send(dSC, &r, sizeof(int), 0) < 0)
    {
      perror("Problème d'envoi depuis le serveur");
      return 0;
    }
    printf("Message Envoyé\n");
  }
  shutdown(dSC, 2);
  shutdown(dS, 2);
  printf("Fin du programme\n");
}
