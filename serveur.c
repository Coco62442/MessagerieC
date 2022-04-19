#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>


struct args {
    int adrC1;
    int adrC2;
};

// argv[1] = port

<<<<<<< HEAD
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
=======
void *client1To2(void * arg) {
	int dSC1 = ((struct args*)arg)->adrC1;
	int dSC2 = ((struct args*)arg)->adrC2;
	int taille_chaine;
	while(1) {
		int error_recv1 = recv(dSC1, &taille_chaine, sizeof(int), 0);
		if (error_recv1 < 0) {
			perror("Erreur lors de la réception du message\n");
			return 0;
		}
		printf("Message reçu : %d\n", taille_chaine) ;

		int error_send1 = send(dSC2, &taille_chaine, sizeof(int), 0);
		if (error_send1 < 0) {
			perror("Le message n'a pas pu être envoyé\n");
			return 0;
		}
		printf("Message Envoyé\n");

		char *msg = malloc(sizeof(char)*taille_chaine);
		int error_recv2 = recv(dSC1, msg, taille_chaine, 0);
		if (error_recv2 < 0) {
			perror("Erreur lors de la réception du message\n");
			return 0;
		}
		printf("Message reçu : %s\n", msg) ;
		
		int error_send2 = send(dSC2, msg, sizeof(char)*taille_chaine, 0);
		if (error_send2 < 0) {
			perror("Le message n'a pas pu être envoyé\n");
			return 0;
		}
		printf("Message Envoyé\n");
	}
	int pthread_cancel(pthread_t thread_envoie);
	pthread_exit(NULL);
}

void *client2To1(void * arg) {
	int dSC1 = ((struct args*)arg)->adrC1;
	int dSC2 = ((struct args*)arg)->adrC2;
	int taille_chaine;
	while (1) {
		int error_recv1 = recv(dSC2, &taille_chaine, sizeof(int), 0);
		if (error_recv1 < 0) {
			perror("Erreur lors de la réception test du message\n");
			return 0;
		}
		printf("Message reçu : %d\n", taille_chaine) ;

		int error_send1 = send(dSC1, &taille_chaine, sizeof(int), 0);
		if (error_send1 < 0) {
			perror("Le message n'a pas pu être envoyé\n");
			return 0;
		}
		printf("Message Envoyé\n");

		char *msg = malloc(sizeof(char)*taille_chaine);
		int error_recv2 = recv(dSC2, msg, taille_chaine, 0);
		if (error_recv2 < 0) {
			perror("Erreur lors de la réception du message\n");
			return 0;
		}
		printf("Message reçu : %s\n", msg) ;
		
		int error_send2 = send(dSC1, msg, sizeof(char)*taille_chaine, 0);
		if (error_send2 < 0) {
			perror("Le message n'a pas pu être envoyé\n");
			return 0;
		}
		printf("Message Envoyé\n");
	};
	int pthread_cancel(pthread_t thread_client1To2);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

	printf("Début programme\n");

	int dS = socket(PF_INET, SOCK_STREAM, 0);
	printf("Socket Créé\n");


	struct sockaddr_in ad;
	ad.sin_family = AF_INET;
	ad.sin_addr.s_addr = INADDR_ANY ;
	ad.sin_port = htons(atoi(argv[1])) ;
	bind(dS, (struct sockaddr*)&ad, sizeof(ad)) ;
	printf("Socket Nommé\n");

	listen(dS, 7);
	printf("Mode écoute\n");

	struct sockaddr_in aC ;
	socklen_t lg = sizeof(struct sockaddr_in) ;
	int dSC1 = accept(dS, (struct sockaddr*) &aC,&lg);
	if (dSC1 < 0) {
		perror("Erreur lors de l'acceptation du client 1\n");
	}
	printf("Client 1 Connecté\n");

	int dSC2 = accept(dS, (struct sockaddr*) &aC,&lg);
	if (dSC2 < 0) {
		perror("Erreur lors de l'acceptation du client 2\n");
	}
	printf("Client 2 Connecté\n");

	struct args *adr = (struct args *)malloc(sizeof(struct args));
    adr->adrC1 = dSC1;
    adr->adrC2 = dSC2;

	pthread_t thread_client1To2;
	pthread_t thread_client2To1;
	pthread_create(&thread_client1To2, NULL, client1To2, (void *) adr);
	pthread_create(&thread_client2To1, NULL, client2To1, (void *) adr);
	

	pthread_join(thread_client1To2,0);
	pthread_join(thread_client2To1,0);

	shutdown(dSC1, 2) ;
	shutdown(dSC2, 2) ;
>>>>>>> cb6a9977ad52a025f512c603fd348d530df9303d
	shutdown(dS, 2) ;
	printf("Fin du programme\n");
}