#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

// argv[1] = port

int main(int argc, char *argv[]) {

	printf("Début programme\n");

	int dS = socket(PF_INET, SOCK_STREAM, 0);
	printf("Socket 1 Créé\n");

	struct sockaddr_in ad;
	ad.sin_family = AF_INET;
	ad.sin_addr.s_addr = INADDR_ANY ;
	ad.sin_port = htons(atoi(argv[1])) ;
	bind(dS, (struct sockaddr*)&ad, sizeof(ad)) ;
	printf("Socket Nommé\n");

	listen(dS, 7) ;
	printf("Mode écoute\n");

	struct sockaddr_in aC ;
	socklen_t lg = sizeof(struct sockaddr_in) ;
	// creation des sockets
	int dSC1 = accept(dS, (struct sockaddr*) &aC,&lg);
	if (dSC1 < 0) {
		perror("Erreur lors de l'acceptation du premier client\n");
	}
	printf("Client 1 Connecté\n");
	int dSC2 = accept(dS, (struct sockaddr*) &aC,&lg);
	if (dSC2 < 0) {
		perror("Erreur lors de l'acceptation du second client\n");
	}
	printf("Client 2 Connecté\n");

	// reception des messages
	int taille_chaine;
	
	int error_recv = recv(dSC1, &taille_chaine, sizeof(int), 0);
	if (error_recv < 0) {
		perror("Erreur lors de la réception du message\n");
		return 0;
	}
	printf("Message reçu : %d\n", taille_chaine) ;


	char msg = malloc(sizeof(char)*taille_chaine);
	int error_recv2 = recv(dSC1, msg, taille_chaine, 0);
	if (error_recv2 < 0) {
		perror("Erreur lors de la réception du message\n");
		return 0;
	}
	printf("Message reçu : %s\n", msg) ;
	// renvoie du message



	free(msg);

	int r = 10 ;
	
	int error_send = send(dSC2, &r, sizeof(int), 0);
	if (error_send < 0) {
		perror("Le message n'a pas pu être envoyé\n");
		return 0;
	}
	printf("Message Envoyé\n");
		
	shutdown(dSC1, 2) ; 
	shutdown(dS, 2) ;
	printf("Fin du programme\n");
}