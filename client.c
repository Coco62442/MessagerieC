// #######  	CHANGEMENTS  	##############
// les threads sont instancies en global
// si receiving recoit le code de deconnexion serveur receiving s arrete et kill thread sending pr termine le client
// ajout de la variable portServeur
// envoieFichier comme pr le serveur retouchée
// changements ds useOfCommands

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

/**
 * - DOSSIER_ENVOI_FICHIERS = chemin du fichier dans lequel sont stockés les fichiers de transfert
 * - nomFichier = nom du fichier à transférer
 * - isEnd = booléen vérifiant si le client est connecté ou s'il a terminé la discussion avec le serveur
 * - dS = socket du serveur
 * - boolConnect = booléen vérifiant si le client est connecté afin de gérer les signaux (CTRL+C)
 * - thread_files = thread gérant le transfert de fichiers
 * - addrServeur = adresse du serveur sur laquelle est connecté le client
 * - portServeur = port du serveur sur lequel est connecté le client
 * - aS = structure contenant toutes les informations de connexion du client au serveur
 */
char *DOSSIER_ENVOI_FICHIERS = "./fichiers_client";
char nomFichier[20];
int isEnd = 0;
int dS = -1;
int boolConnect = 0;
char *addrServeur;
int portServeur;
struct sockaddr_in aS;

// Création des threads
pthread_t thread_sending;
pthread_t thread_receiving;

/**
 * @brief Vérifie si un client souhaite quitter la communication.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client veut quitter, 0 sinon.
 */
int endOfCommunication(char *msg)
{
	if (strcmp(msg, "/fin\n") == 0)
	{
		return 1;
	}
	return 0;
}

/**
 * @brief Envoie un message au serveur et teste que tout se passe bien.
 *
 * @param msg message à envoyer
 */
void sending(char *msg)
{
	if (send(dS, msg, strlen(msg) + 1, 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}
}

/**
 * @brief Fonction principale du thread gérant le transfert de fichiers vers le serveur.
 */
void *envoieFichier()
{
	char *fileName = nomFichier;

	// Création de la socket
	int dS_file = socket(PF_INET, SOCK_STREAM, 0);

	if (dS_file == -1)
	{
		perror("Problème de création de socket client\n");
		exit(-1);
	}

	// Nommage de la socket
	aS.sin_family = AF_INET;
	inet_pton(AF_INET, addrServeur, &(aS.sin_addr));
	aS.sin_port = htons(portServeur + 1);
	socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
	printf("Connection en cours...\n");
	if (connect(dS_file, (struct sockaddr *)&aS, lgA) < 0)
	{
		perror("Problème de connexion au serveur\n");
		exit(-1);
	}
	printf("Socket connectée\n");

	// DEBUT ENVOI FICHIER
	char *path = malloc(sizeof(char) * 40);
	strcpy(path, "fichiers_client/");
	strcat(path, fileName);

	FILE *stream = fopen(path, "r");
	if (stream == NULL)
	{
		fprintf(stderr, "[ENVOI FICHIER] Cannot open file for reading\n");
		exit(-1);
	}

	fseek(stream, 0, SEEK_END);
	int length = ftell(stream);
	fseek(stream, 0, SEEK_SET);

	// Lecture et stockage pour envoi du fichier
	char *toutFichier = malloc(sizeof(char) * length);
	int tailleFichier = fread(toutFichier, sizeof(char), length, stream);
	free(path);
	fclose(stream);

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
	if (send(dS_file, toutFichier, sizeof(char) * tailleFichier, 0) == -1)
	{
		perror("Erreur au send");
		exit(-1);
	}
}

/**
 * @brief Fonction principale du thread gérant le transfert de fichier depuis le serveur.
 *
 * @param ds socket du serveur
 */
void *receptionFichier(void *ds)
{

	int ds_file = (long)ds;
	int returnCode;
	int index;

	char *fileName = malloc(sizeof(char) * 100);
	int tailleFichier;

	if (recv(ds_file, &tailleFichier, sizeof(int), 0) == -1)
	{
		perror("Erreur au recv de fichier\n** fin de la communication **\n");
		exit(-1);
	}

	if (recv(ds_file, fileName, sizeof(char) * 100, 0) == -1)
	{
		printf("Erreur au recv de fichier\n** fin de la communication **\n");
		exit(-1);
	}

	char *buffer = malloc(sizeof(char) * tailleFichier);

	if (recv(ds_file, buffer, sizeof(char) * tailleFichier, 0) == -1)
	{
		printf("Erreur au recv de fichier\n** fin de la communication **\n");
		exit(-1);
	}
	printf("%s\n", buffer);
	printf("Taille fichier = %d\n", tailleFichier);

	char *emplacementFichier = malloc(sizeof(char) * 120);
	strcpy(emplacementFichier, "./fichiers_client/");
	strcat(emplacementFichier, fileName);
	FILE *stream = fopen(emplacementFichier, "w");
	if (stream == NULL)
	{
		fprintf(stderr, "Cannot open file for writing\n");
		exit(EXIT_FAILURE);
	}

	if (tailleFichier != fwrite(buffer, sizeof(char), tailleFichier, stream))
	{
		fprintf(stderr, "Cannot write block in file\n");
		exit(EXIT_FAILURE);
	}

	fseek(stream, 0, SEEK_END);
	int length = ftell(stream);
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
	sleep(0.2);
	free(buffer);
	sleep(0.2);
	free(emplacementFichier);
	shutdown(ds_file, 2);
}

/**
 * @brief Vérifie si un client souhaite utiliser une des commandes
 * disponibles.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client utilise une commande, 0 sinon.
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

		int rep;
		scanf("%d", &rep);
		printf("Fichier voulu %d\n", rep);
		while (rep < 0 || rep >= files)
		{
			printf("REP1 :%d\n", rep);
			printf("Veuillez entrer un numéro valide\n");
			scanf("%d", &rep);
			printf("REP2 :%d\n", rep);
		}

		printf("%s\n", tabFichier[rep]);
		strcpy(nomFichier, tabFichier[rep]);

		printf("%s\n", nomFichier);

		pthread_t thread_envoieFichier;

		if (pthread_create(&thread_envoieFichier, NULL, envoieFichier, 0) < 0)
		{
			perror("Erreur de création de thread de récéption client\n");
			exit(-1);
		}

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
		aS.sin_port = htons(portServeur + 1);
		socklen_t lgA = sizeof(struct sockaddr_in);

		// Envoi d'une demande de connexion
		printf("Connection en cours...\n");
		if (connect(dS_file, (struct sockaddr *)&aS, lgA) < 0)
		{
			perror("Problème de connexion au serveur\n");
			exit(-1);
		}
		printf("Socket connectée\n");

		char *listeFichier = malloc(sizeof(char) * 300);
		if (recv(dS_file, listeFichier, sizeof(char) * 300, 0) == -1)
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

		pthread_t thread_receiving_file;
		if (pthread_create(&thread_receiving_file, NULL, receptionFichier, (void *)(long)dS_file) < 0)
		{
			perror("Erreur de création de thread de récéption client\n");
			exit(-1);
		}

		return 1;
	}

	return 0;
}

/**
 * @brief Fonction principale pour le thread gérant l'envoi de messages.
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

/**
 * @brief Réceptionne un message du serveur et teste que tout se passe bien.
 *
 * @param rep buffer contenant le message reçu
 * @param size taille maximum du message à recevoir
 */
void receiving(char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		printf("** fin de la communication **\n");
		exit(-1);
	}
}

/**
 * @brief Fonction principale pour le thread gérant la réception de messages.
 */
void *receivingForThread()
{
	while (!isEnd)
	{
		char *r = (char *)malloc(sizeof(char) * 100);
		receiving(r, sizeof(char) * 100);
		if (strcmp(r, "Tout ce message est le code secret pour désactiver les clients") == 0)
		{
			free(r);
			break;
		}

		printf("%s", r);
		free(r);
	}
	shutdown(dS, 2);
	pthread_cancel(thread_sending);
	return NULL;
}

/**
 * @brief Fonction gérant l'interruption du programme par CTRL+C.
 * Correspond à la gestion des signaux.
 *
 * @param sig_num numéro du signal
 */
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
	portServeur = atoi(argv[2]);

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
	do
	{
		printf("Votre pseudo (maximum 11 caractères):\n");
		fgets(myPseudo, 12, stdin);
	} while (strcmp(myPseudo, "\n") == 0);

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

	return 1;
}