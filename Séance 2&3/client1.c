#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LENGTH 10

/*
 * Envoie un message à une socket et teste que tout se passe bien
 * Paramètres : int dS : la socket
 *              char * msg : message à envoyer
*/
void sending(int dS, char *msg)
{
  if (send(dS, msg, strlen(msg) + 1, 0) == -1)
  {
    perror("Erreur au send");
    exit(-1);
  }
}

/*
 * Receptionne un message d'une socket et teste que tout se passe bien
 * Paramètres : int dS : la socket
 *              char * msg : message à recevoir
 *              ssize_t size : taille maximum du message à recevoir
*/
void receiving(int dS, char *rep, ssize_t size)
{
  if (recv(dS, rep, size, 0) == -1)
  {
    printf("** fin de la communication **\n");
    exit(-1);
  }
}

/*
 * Vérifie si un client souhaite quitter la communication
 * Paramètres : char ** msg : message du client à vérifier
 * Retour : 1 (vrai) si le client veut quitter, 0 (faux) sinon
 */
int endOfCommunication(char *msg)
{
  if (strcmp(msg, "fin\n") == 0)
  {
    strcpy(msg, "** a quitté la communication **\n");
    return 1;
  }
  return 0;
}


// argv[1] = adresse ip
// argv[2] = port
int main(int argc, char *argv[])
{
  if (argc < 3)
  {
    perror("Erreur : Lancez avec ./client [votre_ip] [votre_port] ");
    return -1;
  }
  printf("Début programme\n");

  // Création de la socket
  int dS = socket(PF_INET, SOCK_STREAM, 0);
  if (dS == -1)
  {
    perror("Problème de création de socket client");
    return -1;
  }
  printf("Socket Créé\n");

  // Nommage de la socket
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

  char *buffer = (char *) malloc(MAX_LENGTH);
  int taille = 0;
  while (1)
  {
    printf("Entrer un message de taille max %d: \n", MAX_LENGTH - 1);
    fgets(buffer, MAX_LENGTH, stdin);
    taille = strlen(buffer);
    printf("Taille du message : %d\n", taille - 1);

    if (send(dS, &taille, sizeof(int), 0) < 0)
    {
      perror("Problème d'envoi de la taille");
      return -1;
    }
    printf("Taille Envoyée \n");

    if (send(dS, buffer, taille, 0) < 0)
    {
      perror("Problème d'envoi du message");
      return -1;
    }
    printf("Message Envoyé \n");
  }
  free(buffer);
  shutdown(dS, 2);
  printf("Fin du programme\n");
}