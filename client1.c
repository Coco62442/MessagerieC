#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

// argv[1] = adresse ip
// argv[2] = port

int main(int argc, char *argv[]) {

  printf("Début programme\n");
  int dS = socket(PF_INET, SOCK_STREAM, 0);
  printf("Socket Créé\n");

  struct sockaddr_in aS;
  aS.sin_family = AF_INET;
  inet_pton(AF_INET,argv[1],&(aS.sin_addr));
  aS.sin_port = htons(atoi(argv[2])) ;
  socklen_t lgA = sizeof(struct sockaddr_in) ;
  int error_connect = connect(dS, (struct sockaddr *) &aS, lgA);
  if (error_connect < 0) {
	  perror("Erreur lors de la connection au serveur\nVérifier l\'adresse IP fournie");
	  return 0;
  };
  printf("Socket Connecté\n");

  // Message a envoyé
  
  char * chaine = malloc(10*sizeof(char));
  printf("Entrer un mot de 9 caractères maximum\n");
  fgets(chaine, 10, stdin);
  
  int taille_chaine = strlen(chaine);
		
	int error_send = send(dS, &taille_chaine, sizeof(int) , 0) ;
	if (error_send < 0) {
		perror("Erreur lors de l\'envoie du message au serveur");
	};
	printf("Message Envoyé \n");


	int error_send2 = send(dS, chaine, strlen(chaine) + 1 , 0) ;
	if (error_send2 < 0) {
		perror("Erreur lors de l\'envoie du message au serveur");
	};
	printf("Message Envoyé \n");

  free(chaine);
  shutdown(dS,2) ;
  printf("Fin du programme\n");
}