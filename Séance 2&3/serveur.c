#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*
* Définition d'une structure Client pour regrouper toutes les informations du client
*/
typedef struct Client Client;
struct Client{
    int isOccupied;
    //char * pseudo;
    long dSC;
};

//TODO: à retoucher
/* - MAX_CLIENT = nombre maximum de client accepté sur le serveur
 * - tabClient = tableau répertoriant les clients connectés
 * - tabThread = tableau des threads associés au traitement de chaque client
 * - nbClient = nombre de clients actuellement connectés*/
#define MAX_CLIENT 5
Client tabClient[MAX_CLIENT];
pthread_t tabThread[MAX_CLIENT];
long nbClient = 0;

/*
* Fonctions pour gérer les indices du tableaux de clients 
* Retour : un entier, indice du premier emplacement disponible
*          -1 si tout les emplecements sont occupés. 
*/
int giveNumClient(){
    int i = 0;
    while (i<MAX_CLIENT){
        if(!tabClient[i].isOccupied){
            return i;
        }
        i+=1;
    }
    return -1;
}

/*
 * Envoi un message à toutes les sockets présentent dans le tableau des clients
 * et teste que tout se passe bien
 * Paramètres : int dS : expéditeur du message
 *              char * msg : message à envoyer
 * Retour : pas de retour
*/
void sending(int dS, char * msg){
    int i;
    for (i = 0; i<MAX_CLIENT ; i++) {
        /*On n'envoie pas au client qui a écrit le message*/
        if(tabClient[i].isOccupied && dS != tabClient[i].dSC){
            int sendR = send(tabClient[i].dSC, msg, strlen(msg)+1, 0);
            if (sendR == -1){ /*vérification de la valeur de retour*/
                perror("erreur au send");
                exit(-1);
            }
        }
    }
}

/*
 * Receptionne un message d'une socket et teste que tout se passe bien
 * Paramètres : int dS : la socket
 *              char * msg : message à recevoir
 *              ssize_t size : taille maximum du message à recevoir
 * Retour : pas de retour
*/
void receiving(int dS, char * rep, ssize_t size){
    int recvR = recv(dS, rep, size, 0);
    if (recvR == -1){ /*vérification de la valeur de retour*/
        perror("erreur au recv");
        exit(-1);
    }
}



/*
 * _____________________ MAIN _____________________
*/
// argv[1] = port
int main(int argc, char *argv[])
{
  // Verification du nombre de paramètres
  if(argc<2){
    perror("Erreur : Lancez avec ./serveur <votre_port> ");
    return -1;// TODO: ou exit(-1); ?
  }
  printf("Début programme\n");

  // Création de la socket
  int dS = socket(PF_INET, SOCK_STREAM, 0);
  if (dS < 0)
  {
    perror("Problème de création de socket serveur");
    return -1;
  }
  printf("Socket Créé\n");

  // Nommage de la socket
  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_addr.s_addr = INADDR_ANY;
  ad.sin_port = htons(atoi(argv[1]));
  if (bind(dS, (struct sockaddr *)&ad, sizeof(ad)) < 0) {
    perror("Erreur lors du nommage de la socket");
    return -1;
  }
  printf("Socket nommée\n");

  // Passage de la socket en mode écoute
  if (listen(dS, 7) < 0) {
    perror("Problème au niveau du listen");
    return -1;
  }
  printf("Mode écoute\n");

  int taille = -1;
  while (1)
  {
    int dSC;
    // Vérifions si on peut accepter un client
    if (nbClient < MAX_CLIENT) {

      // Acceptons une connexion
      struct sockaddr_in aC;
      socklen_t lg = sizeof(struct sockaddr_in);
      dSC = accept(dS, (struct sockaddr *)&aC, &lg);
      if (dSC < 0)
      {
        perror("Problème lors de l'acceptation du client 1");
        return -1;
      }
      printf("Client %d connecté\n", nbClient);

    }

    long numClient = giveNumClient();

    /*On enregistre la socket du client*/
    tabClient[numClient].isOccupied = 1;
    tabClient[numClient].dSC = dSC;


    /*_____________________ Communication _____________________*/
    if(pthread_create(&tabThread[numClient],NULL,broadcast,(void *)numClient) == -1){
        perror("erreur thread create");
    }

    /*On a un client en plus sur le serveur, on incrémente*/
    nbClient += 1;
    
    printf("Clients connectés : %d\n", nbClient);


    // // TODO: DEBUT
    // if (pthread_create(&thread, NULL, envoie, (void *) 0)){ //thread Client 1 vers Client 2
    //   perror("creation threadGet erreur");
		// 	return EXIT_FAILURE;
		// }

    // // Gestion de la fin
    // if (pthread_join(thread, NULL)) {
    //   perror("Erreur fermeture thread");
    //   return -1;
    // }
    // pthread_cancel(thread);
    // // TODO: FIN

    // // Réception du client 1
    // if (recv(dSC1, &taille, sizeof(int), 0) < 0 || taille < 0)
    // {
    //   perror("Problème de réception de taille depuis le serveur");
    //   return -1;
    // }
    // printf("Taille reçue : %d\n", taille - 1);

    // char *msg = malloc(sizeof(char) * taille);
    // if (recv(dSC1, msg, taille, 0) < 0)
    // {
    //   perror("Problème de réception du message depuis le serveur");
    //   return -1;
    // }
    // printf("Message reçu : %s\n", msg);

    // // Envoi au client 2
    // if (send(dSC2, &taille, sizeof(int), 0) < 0)
    // {
    //   perror("Problème d'envoi de la taille depuis le serveur au client 2");
    //   return -1;
    // }
    // printf("Taille envoyée au client 2\n");

    // if (send(dSC2, msg, sizeof(char) * taille, 0) < 0)
    // {
    //   perror("Problème d'envoi depuis le serveur au client 2");
    //   return -1;
    // }
    // printf("Message Envoyé au client 2\n");
  }
  // shutdown(dSC1, 2);
  // shutdown(dSC2, 2);
  shutdown(dS, 2);
  printf("Fin du programme\n");
}
