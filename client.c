#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#define SIZE 1024

char *addrServeur;
int isEnd = 0;
int dS = -1;
int boolConnect = 0;
pthread_t thread_files;
struct sockaddr_in aS;

/*
 * Vérifie si un client souhaite quitter la communication
 * Paramètres : char ** msg : message du client à vérifier
 * Retour : 1 (vrai) si le client veut quitter, 0 (faux) sinon
 */
int endOfCommunication(char *msg)
{
	if (strcmp(msg, "/fin\n") == 0)
	{
		return 1;
	}
	return 0;
}

/*
 * Envoie un message à une socket et teste que tout se passe bien
 * Paramètres : int dS : la socket
 *              char * msg : message à envoyer
 */
void sending(char *msg)
{
	if (send(dS, msg, strlen(msg) + 1, 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}
}

void *sendFileForThread(void *filename)
{
	// Création de la socket
	int dS_file = socket(PF_INET, SOCK_STREAM, 0);
	if (dS_file == -1)
	{
		perror("[ENVOI FICHIER] Problème de création de socket client\n");
		exit(-1);
	}
	printf("[ENVOI FICHIER] Socket Créé\n");

	// Nommage de la socket
    struct sockaddr_in aS_fileTransfer;
    aS_fileTransfer.sin_family = AF_INET;
    inet_pton(AF_INET, addrServeur, &(aS_fileTransfer.sin_addr));
    aS_fileTransfer.sin_port = htons(atoi("3000") + 1);
    socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
    printf("[FICHIER] Connection en cours...\n");
    if (connect(dS_file, (struct sockaddr *)&aS_fileTransfer, lgA) < 0)
    {
        perror("[FICHIER] Problème de connexion au serveur\n");
        exit(-1);
    }
	printf("[ENVOI FICHIER] Socket connectée\n");

	// DEBUT ENVOI FICHIER
	char *path = malloc(100);
	strcat(path, "./FichierClient/");
	strcat(path, filename);
	FILE *stream = fopen(path, "r");
	if (stream == NULL)
	{
		fprintf(stderr, "[ENVOI FICHIER] Cannot open file for reading\n");
		exit(-1);
	}
	fseek(stream, 0, SEEK_END);
	int length = ftell(stream);
	printf("[FICHIER] Taille du fichier : %d\n", length);
	fseek(stream, 0, SEEK_SET);

	 // Envoi de la taille du fichier, puis de son nom
    if (send(dS_file, &length, sizeof(int), 0) == -1)
    {
        perror("Erreur au send");
        exit(-1);
    }
    if (send(dS_file, filename, strlen(filename) + 1, 0) == -1)
    {
        perror("Erreur au send");
        exit(-1);
    }

	// Lecture et stockage pour envoi du fichier
	char *chaine = malloc(100);
	char *toutFichier = malloc(length);
	while (fgets(chaine, 100, stream) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
	{
		strcat(toutFichier, chaine);
	}
	if (send(dS_file, toutFichier, length + 1, 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}
	free(chaine);
	free(toutFichier);
	fclose(stream);
	shutdown(dS_file, 2);
}

void *recvFileForThread(void *filename)
{
	// Création de la socket
	int dS_file = socket(PF_INET, SOCK_STREAM, 0);
	if (dS_file == -1)
	{
		perror("[ENVOI FICHIER] Problème de création de socket client\n");
		exit(-1);
	}
	printf("[ENVOI FICHIER] Socket Créé\n");

	// Nommage de la socket
    struct sockaddr_in aS_fileTransfer;
    aS_fileTransfer.sin_family = AF_INET;
    inet_pton(AF_INET, addrServeur, &(aS_fileTransfer.sin_addr));
    aS_fileTransfer.sin_port = htons(atoi("3000") + 1);
    socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
    printf("[FICHIER] Connection en cours...\n");
    if (connect(dS_file, (struct sockaddr *)&aS_fileTransfer, lgA) < 0)
    {
        perror("[FICHIER] Problème de connexion au serveur\n");
        exit(-1);
    }
	printf("[ENVOI FICHIER] Socket connectée\n");

	char *listeFichiers = (char *)malloc(sizeof(char) * 100);
	if (recv(dS_file, listeFichiers, sizeof(char) * 100, 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
	printf("%s", listeFichiers);
	char *rep = malloc(sizeof(int));
	fgets(rep, 2, stdin);
	if (send(dS_file, rep, strlen(rep) + 1, 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}

	// Réception des informations du fichier
	int tailleFichier;
	char *nomFichier = (char *)malloc(sizeof(char) * 100);
	if (recv(dS_file, &tailleFichier, sizeof(int), 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
	if (recv(dS_file, nomFichier, sizeof(char) * 100, 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
	printf("%s\n", nomFichier);
	printf("%d\n", tailleFichier);

	// Début réception du fichier
	char *buffer = (char *)malloc(sizeof(char) * tailleFichier);
	int returnCode;
	int index;

	char *emplacementFichier = (char *)malloc(sizeof(char) * 30);
	strcat(emplacementFichier, "FichierClient/");
	strcat(emplacementFichier, nomFichier);
	FILE *stream = fopen(emplacementFichier, "w");
	if (stream == NULL)
	{
		fprintf(stderr, "Cannot open file for writing\n");
		exit(-1);
	}

	if (recv(dS_file, buffer, sizeof(int), 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
	printf("%s\n", buffer);

	strcat(buffer, "\n");

	if (1 != fwrite(buffer, tailleFichier + 1, 1, stream))
	{
		fprintf(stderr, "Cannot write block in file\n");
	}

	returnCode = fclose(stream);
	if (returnCode == EOF)
	{
		fprintf(stderr, "Cannot close file\n");
		exit(-1);
	}
	printf("kikoo\n");
	free(buffer);
	free(nomFichier);
	free(emplacementFichier);

	printf("Téléchargement du fichier terminé\n");
	shutdown(dS_file, 2);
}

/*
 * Envoie le fichier donné en paramètre au serveur
 * Paramètres : FILE *fp : le fichier à envoyer
 *              int dS : la socket du serveur
 */
void send_file(char *filename)
{
	pthread_create(&thread_files, NULL, sendFileForThread, (void *)filename);
}

/*
 * Vérifie si le client souhaite utiliser une des commandes
 * Paramètres : char *msg : message du client à vérifier
 * Retour : 1 (vrai) si le client utilise une commande, 0 (faux) sinon
 */
int useOfCommand(char *msg)
{
	if (strcmp(msg, "/déposer\n") == 0)
	{
		sending("/déposer\n");
		send_file("test.txt");
		return 1;
	}
	else if (strcmp(msg, "/télécharger\n") == 0)
	{
		
		sending("/télécharger\n");
		pthread_t threadRecvFile;

		if (pthread_create(&threadRecvFile, NULL, recvFileForThread, NULL) < 0 )
		{
			perror("Erreur\n");
			exit(-1);
		}
		return 1;
	}
	
	return 0;
}

/*
 * Fonction pour le thread d'envoi
 */
void *sendingForThread()
{
	while (!isEnd)
	{
		/*Saisie du message au clavier*/
		char *m = (char *)malloc(sizeof(char) * 100);
		fgets(m, 100, stdin);

		// On vérifie si le client veut quitter la communication
		isEnd = endOfCommunication(m);

		// On vérifie si le client utilise une des commandes
		char *msgToVerif = (char *)malloc(sizeof(char) * strlen(m));
		strcpy(msgToVerif, m);
		if (useOfCommand(msgToVerif))
		{
			free(m);
			continue;
		}

		// Envoi
		sending(m);
		free(m);
	}
	shutdown(dS, 2);
	return NULL;
}

/*
 * Receptionne un message d'une socket et teste que tout se passe bien
 * Paramètres : int dS : la socket
 *              char * msg : message à recevoir
 *              ssize_t size : taille maximum du message à recevoir
 */
void receiving(char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		printf("** fin de la communication **\n");
		exit(-1);
	}
}

/*
 * Fonction pour le thread de réception
 */
void *receivingForThread()
{
	while (!isEnd)
	{
		char *r = (char *)malloc(sizeof(char) * 100);
		receiving(r, sizeof(char) * 100);
		// strcpy(r, strcat(r, "\n4 > "));
		printf("%s", r);
		free(r);
	}
	shutdown(dS, 2);
	return NULL;
}

/* Signal Handler for SIGINT */
void sigintHandler(int sig_num)
{
	printf("\nFin du programme avec Ctrl + C \n");
	if (!boolConnect)
	{
		char *myPseudoEnd = (char *)malloc(sizeof(char) * 12);
		myPseudoEnd = "FinClient";
		sending(myPseudoEnd);
	}
	sleep(0.2);
	sending("** a quitté la communication **\n");
	exit(1);
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

	addrServeur = argv[1];

	// Création de la socket
	dS = socket(PF_INET, SOCK_STREAM, 0);
	if (dS == -1)
	{
		perror("Problème de création de socket client\n");
		return -1;
	}
	printf("Socket Créé\n");

	// Nommage de la socket
	aS.sin_family = AF_INET;
	inet_pton(AF_INET, argv[1], &(aS.sin_addr));
	aS.sin_port = htons(atoi(argv[2]));
	socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
	printf("Connection en cours...\n");
	if (connect(dS, (struct sockaddr *)&aS, lgA) < 0)
	{
		perror("Problème de connexion au serveur\n");
		exit(-1);
	}
	printf("Socket connectée\n");

	// Fin avec Ctrl + C
	signal(SIGINT, sigintHandler);

	// Saisie du pseudo du client au clavier
	char *myPseudo = (char *)malloc(sizeof(char) * 12);
	printf("Votre pseudo (maximum 11 caractères):\n");
	fgets(myPseudo, 12, stdin);

	// Envoie du pseudo
	sending(myPseudo);

	char *repServeur = (char *)malloc(sizeof(char) * 60);
	// Récéption de la réponse du serveur
	receiving(repServeur, sizeof(char) * 60);
	printf("%s\n", repServeur);

	while (strcmp(repServeur, "Pseudo déjà existant\n") == 0)
	{
		// Saisie du pseudo du client au clavier
		printf("Votre pseudo (maximum 11 caractères)\n");
		fgets(myPseudo, 12, stdin);

		// Envoie du pseudo
		sending(myPseudo);

		// Récéption de la réponse du serveur
		receiving(repServeur, sizeof(char) * 60);
		printf("%s\n", repServeur);
	}
	free(myPseudo);
	printf("Connection complète\n");
	boolConnect = 1;

	//_____________________ Communication _____________________
	// Création des threads
	pthread_t thread_sendind;
	pthread_t thread_receiving;

	if (pthread_create(&thread_sendind, NULL, sendingForThread, 0) < 0)
	{
		perror("Erreur de création de thread d'envoi client\n");
		exit(-1);
	}

	if (pthread_create(&thread_receiving, NULL, receivingForThread, 0) < 0)
	{
		perror("Erreur de création de thread réception client\n");
		exit(-1);
	}

	pthread_join(thread_sendind, NULL);
	pthread_join(thread_receiving, NULL);

	printf("Fin du programme\n");
}