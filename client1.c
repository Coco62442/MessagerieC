#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// argv[1] = adresse ip
// argv[2] = port

void *envoieMsg(void * arg) {
	int dS = (int) arg;
	char * chaine = malloc(200*sizeof(char));
	do {
		// Message a envoyé

		printf("Entrer votre message\n");
		fgets(chaine, 200, stdin);

		int tailleChaineEnvoie = strlen(chaine);
			
		int error_send = send(dS, &tailleChaineEnvoie, sizeof(int) , 0) ;
		if (error_send < 0) {
			perror("Erreur lors de l\'envoie du message au serveur");
		};
		printf("Message Envoyé \n");


		int error_send2 = send(dS, chaine, strlen(chaine) + 1 , 0) ;
		if (error_send2 < 0) {
			perror("Erreur lors de l\'envoie du message au serveur");
		};
		printf("Message Envoyé \n");

	} while (!strcmp(chaine, "fin"));
	free(chaine);
	int pthread_cancel(pthread_t thread_recu);
	pthread_exit(NULL);
}



void *recuMsg(void * arg) {
	int dS = (int) arg;	
	char * rep;
	do {
		int tailleChaineRecu;
		int error_recv1 = recv(dS, &tailleChaineRecu, sizeof(int), 0);
		if (error_recv1 < 0) {
			perror("Erreur le message n\'a pas été reçu");
		};
		printf("Réponse reçue : %d\n", tailleChaineRecu);


		rep = malloc(sizeof(char)*tailleChaineRecu);
		int error_recv2 = recv(dS, rep, sizeof(char)*tailleChaineRecu, 0);
		if (error_recv2 < 0) {
			perror("Erreur le message n\'a pas été reçu");
		};
		printf("Réponse reçue : %s\n", rep) ;

		free(rep);
	} while (!strcmp(rep, "fin"));
	int pthread_cancel(pthread_t thread_envoie);
	pthread_exit(NULL);
}

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

	pthread_t thread_envoie;
	pthread_t thread_recu;
	pthread_create(&thread_envoie, NULL, envoieMsg, (void *) (int) dS);
	pthread_create(&thread_recu, NULL, recuMsg, (void *) (int) dS);
	

	pthread_join(thread_envoie,0);
	pthread_join(thread_recu,0);
	
	printf("Fin du programme\n");
	return 0;
}