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

		char * rep = malloc(sizeof(char)*5);
		int error_recv = recv(dS, rep, sizeof(char)*5, 0);
		if (error_recv < 0) {
			perror("Erreur le message n\'a pas été reçu");
		};
		printf("Réponse reçue : %s\n", rep) ;

  free(rep);
  shutdown(dS,2) ;
  printf("Fin du programme\n");
}