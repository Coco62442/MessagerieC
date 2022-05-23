#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

char nomFichier[20];
char *addrServeur;
int isEnd = 0;
int dS = -1;
int boolConnect = 0;
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

void *envoieFichier()
{
	printf("Nom : %s\n", nomFichier);
	char *fileName = nomFichier;
	printf("fichier : %s\n", fileName);
	// DEBUT ENVOI FICHIER
	char *path = malloc(sizeof(char) * 50);
	strcpy(path, "fichiers_client/");
	strcat(path, fileName);
	printf("%s\n", path);
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

	printf("Nom du fichier choisi : %s\n", fileName);

	// Lecture et stockage pour envoi du fichier
	char *toutFichier = malloc(length);
	fread(toutFichier, sizeof(char) * length, 1, stream);
	free(path);
	fclose(stream);

	printf("\n\nFichier obtenue : %s\n", toutFichier);

	// Création de la socket
	int dS_file = socket(PF_INET, SOCK_STREAM, 0);
	if (dS_file == -1)
	{
		perror("Problème de création de socket client\n");
		exit(-1);
	}
	printf("[FICHIER] Socket Créé\n");

	// Nommage de la socket
	aS.sin_family = AF_INET;
	inet_pton(AF_INET, addrServeur, &(aS.sin_addr));
	aS.sin_port = htons(3001);
	socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
	printf("Connection en cours...\n");
	if (connect(dS_file, (struct sockaddr *)&aS, lgA) < 0)
	{
		perror("Problème de connexion au serveur\n");
		exit(-1);
	}
	printf("Socket connectée\n");

	if (send(dS_file, &length, sizeof(int), 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}
	if (send(dS_file, fileName, sizeof(char) * 20, 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}
	if (send(dS_file, toutFichier, sizeof(char) * length, 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}
}

void *receptionFichier(void *ds)
{

	int ds_file = (long)ds;
	int returnCode;
	int index;

	char *fileName = malloc(sizeof(char) * 20);
	int tailleFichier;

	if (recv(ds_file, &tailleFichier, sizeof(int), 0) == -1)
	{
		printf("** fin de la communication **\n");
		exit(-1);
	}
	printf("Taille : %d\n", tailleFichier);
	if (recv(ds_file, fileName, sizeof(char) * 20, 0) == -1)
	{
		printf("** fin de la communication **\n");
		exit(-1);
	}
	printf("Nom : %s\n", fileName);

	char *buffer = malloc(sizeof(char) * tailleFichier);

	if (recv(ds_file, buffer, sizeof(char) * tailleFichier, 0) == -1)
	{
		printf("** fin de la communication **\n");
		exit(-1);
	}

	char *emplacementFichier = malloc(sizeof(char) * 200);
	strcpy(emplacementFichier, "./fichiers_client/");
	strcat(emplacementFichier, fileName);
	FILE *stream = fopen(emplacementFichier, "w");
	if (stream == NULL)
	{
		fprintf(stderr, "Cannot open file for writing\n");
		exit(EXIT_FAILURE);
	}

	printf("%s\n", buffer);

	if (1 != fwrite(buffer, sizeof(char) * tailleFichier, 1, stream))
	{
		fprintf(stderr, "Cannot write block in file\n");
		exit(EXIT_FAILURE);
	}

	fseek(stream, 0, SEEK_END);
	int length = ftell(stream);
	printf("[FICHIER] Taille du fichier : %d\n", length);
	printf("[FICHIER] Taille du fichier : %d\n", tailleFichier);
	fseek(stream, 0, SEEK_SET);

	returnCode = fclose(stream);
	if (returnCode == EOF)
	{
		fprintf(stderr, "Cannot close file\n");
		exit(-1);
	}

	if (tailleFichier != length)
	{
		remove(emplacementFichier);
		shutdown(ds_file, 2);
		printf("Un problème est survenue\n");
		printf("Veuillez choisir de nous le fichier que vous désirez\n");
		useOfCommand("/télécharger\n");
	}

	free(fileName);
	free(buffer);
	free(emplacementFichier);
	shutdown(ds_file, 2);
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
		sending(msg);

		char *tabFichier[50];
		DIR *folder;
		struct dirent *entry;
		int files = 0;

		folder = opendir("fichiers_client");
		if (folder == NULL)
		{
			perror("Unable to read directory");
			exit(-1);
		}
		entry = readdir(folder);
		entry = readdir(folder);
		while ((entry = readdir(folder)))
		{

			tabFichier[files] = entry->d_name;
			printf("File %d: %s\n",
				   files,
				   entry->d_name);
			files++;
		}

		closedir(folder);

		char *rep = malloc(sizeof(char) * 2);
		fgets(rep, 2, stdin);
		printf("Fichier voulu %s\n", rep);
		printf("%s\n", tabFichier[atoi(rep)]);
		strcpy(nomFichier, tabFichier[atoi(rep)]);

		printf("%s\n", nomFichier);
		pthread_t test;

		pthread_create(&test, NULL, envoieFichier, 0);

		free(rep);
		return 1;
	}
	else if (strcmp(msg, "/télécharger\n") == 0)
	{

		sending(msg);

		// Création de la socket
		int dS_file = socket(PF_INET, SOCK_STREAM, 0);
		if (dS_file == -1)
		{
			perror("Problème de création de socket client\n");
			exit(-1);
		}
		printf("[FICHIER] Socket Créé\n");

		// Nommage de la socket
		aS.sin_family = AF_INET;
		inet_pton(AF_INET, addrServeur, &(aS.sin_addr));
		aS.sin_port = htons(3001);
		socklen_t lgA = sizeof(struct sockaddr_in);

		// Envoi d'une demande de connexion
		printf("Connection en cours...\n");
		if (connect(dS_file, (struct sockaddr *)&aS, lgA) < 0)
		{
			perror("Problème de connexion au serveur\n");
			exit(-1);
		}
		printf("Socket connectée\n");

		char *listeFichier = malloc(sizeof(char) * 200);
		if (recv(dS_file, listeFichier, sizeof(char) * 200, 0) == -1)
		{
			printf("** fin de la communication **\n");
			exit(-1);
		}
		printf("%s", listeFichier);

		char *numFichier = malloc(sizeof(char) * 5);
		fgets(numFichier, 5, stdin);
		printf("numChoisi %s\n", numFichier);
		printf("strlen %ld\n", strlen(numFichier));
		int val = atoi(numFichier);


		if (send(dS_file, &val, sizeof(int), 0) == -1)
		{
			perror("Erreur à l'envoi du mp");
			exit(-1);
		}
		free(numFichier);

		pthread_t thread_sending_file;
		if (pthread_create(&thread_sending_file, NULL, receptionFichier, (void *)(long)dS_file) < 0)
		{
			perror("Erreur de création de thread d'envoi client\n");
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
	sending("/fin\n");
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
	printf("Connexion en cours...\n");
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
	printf("Connexion complète\n");
	boolConnect = 1;

	//_____________________ Communication _____________________
	// Création des threads
	pthread_t thread_sending;
	pthread_t thread_receiving;

	if (pthread_create(&thread_sending, NULL, sendingForThread, 0) < 0)
	{
		perror("Erreur de création de thread d'envoi client\n");
		exit(-1);
	}

	if (pthread_create(&thread_receiving, NULL, receivingForThread, 0) < 0)
	{
		perror("Erreur de création de thread réception client\n");
		exit(-1);
	}

	pthread_join(thread_sending, NULL);
	pthread_join(thread_receiving, NULL);

	printf("Fin du programme\n");
}