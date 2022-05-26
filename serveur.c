// #######  	CHANGEMENT  	##############
// variable globale nbFiles disparait (sers a rien)
// mutex qd on incremente nbclient l900 environ


#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

/**
 * @brief Structure Client pour regrouper toutes les informations du client.
 *
 * @param isOccupied 1 si le Client est connecté au serveur ; 0 sinon
 * @param dSC Socket de transmission des messages classiques au Client
 * @param pseudo Appellation que le Client rentre à sa première connexion
 * @param dSCFC Socket de transfert des fichiers
 * @param nomFichier Nomination du fichier choisi par le client pour le transfert
 */
typedef struct Client Client;
struct Client
{
	int isOccupied;
	long dSC;
	char *pseudo;
	long dSCFC;
	char nomFichier[100];
};

/*
 * Définition d'une structure Salon pour regrouper toutes les informations d'un salon
 */
typedef struct Salon Salon;
struct Salon
{
	int idSalon;
	int isOccupiedSalon;
	long dSCS;
	char nom[40];
	char *description;
	int nbPlace;
};

/**
 * - MAX_CLIENT = nombre maximum de clients acceptés sur le serveur
 * - tabClient = tableau répertoriant les clients connectés
 * - tabThread = tableau des threads associés au traitement de chaque client
 * - nbClients = nombre de clients actuellement connectés
 * - dS_file = socket de connexion pour le transfert de fichiers
 * - dS = socket de connexion entre les clients et le serveur
 * - semaphore = sémaphore pour gérer le nombre de clients
 * - semaphoreThread = sémpahore pour gérer les threads
 * - mutex = mutex pour la modification de tabClient[]
 */

#define MAX_CLIENT 3
Client tabClient[MAX_CLIENT];
pthread_t tabThread[MAX_CLIENT];
long nbClient = 0;
int dS_file;
int dS;
sem_t semaphore;
sem_t semaphoreThread;
pthread_mutex_t mutex;

/**
 * @brief Fonctions pour gérer les indices du tableaux de clients.
 *
 * @return un entier, indice du premier emplacement disponible ;
 *         -1 si tout les emplacements sont occupés.
 */
int giveNumClient()
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (!tabClient[i].isOccupied)
		{
			return i;
		}
		i += 1;
	}
	return -1;
}

/**
 * @brief Fonctions pour vérifier que le pseudo est unique.
 *
 * @param pseudo pseudo à vérifier
 * @return un entier ;
 *         1 si le pseudo existe déjà,
 *         0 si le pseudo n'existe pas.
 */
int verifPseudo(char *pseudo)
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (tabClient[i].isOccupied && strcmp(pseudo, tabClient[i].pseudo) == 0)
		{
			return 1;
		}
		i++;
	}
	return 0;
}

/**
 * @brief Envoie un message à toutes les sockets présentes dans le tableau des clients
 * et teste que tout se passe bien.
 *
 * @param dS expéditeur du message
 * @param msg message à envoyer
 */
void sending(int dS, char *msg)
{
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].isOccupied && dS != tabClient[i].dSC)
		{
			if (send(tabClient[i].dSC, msg, strlen(msg) + 1, 0) == -1)
			{
				perror("Erreur au send");
				exit(-1);
			}
		}
	}
}

/**
 * @brief Fonction pour récupérer le dSC (socket client) selon un pseudo donné.
 *
 * @param pseudo pseudo pour lequel on cherche la socket
 * @return socket du client nommé [pseudo] ;
 *         -1 si le pseudo n'existe pas.
 */
long pseudoTodSC(char *pseudo)
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (tabClient[i].isOccupied && strcmp(tabClient[i].pseudo, pseudo) == 0)
		{
			return tabClient[i].dSC;
		}
		i++;
	}
	return -1;
}

/**
 * @brief Envoie un message en privé à un client en particulier
 * et teste que tout se passe bien.
 *
 * @param pseudoReceiver destinataire du message
 * @param msg message à envoyer
 */
void sendingDM(char *pseudoReceiver, char *msg)
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (tabClient[i].isOccupied && strcmp(tabClient[i].pseudo, pseudoReceiver) == 0)
		{
			break;
		}
		i++;
	}
	if (i == MAX_CLIENT)
	{
		perror("Pseudo pas trouvé");
		exit(-1);
	}
	long dSC = tabClient[i].dSC;
	if (send(dSC, msg, strlen(msg) + 1, 0) == -1)
	{
		perror("Erreur à l'envoi du mp");
		exit(-1);
	}
}

/**
 * @brief Receptionne un message d'une socket et teste que tout se passe bien.
 *
 * @param dS socket sur laquelle recevoir
 * @param rep buffer où stocker le message reçu
 * @param size taille maximum du message à recevoir
 */
void receiving(int dS, char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
}

/**
 * @brief Fonction pour vérifier si un client souhaite quitter la communication.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client veut quitter, 0 sinon.
 */
int endOfCommunication(char *msg)
{
	if (strcmp(msg, "/fin\n") == 0)
	{
		strcpy(msg, "** a quitté la communication **\n");
		return 1;
	}
	return 0;
}

/**
 * @brief Fonction permettant de recréer le fichier [nomFichier]
 * avec le contenu [buffer].
 *
 * @param nomFichier appellation du fichier à écrire
 * @param buffer contenu du fichier reçu
 * @param tailleFichier taille du fichier reçu
 */
int ecritureFichier(char *nomFichier, char *buffer, int tailleFichier)
{
	char *tabFichierDossier[50];
	DIR *folder;
	struct dirent *entry;
	int files = 0;

	folder = opendir("fichiers_serveur");
	if (folder == NULL)
	{
		perror("Unable to read directory");
		exit(EXIT_FAILURE);
	}

	while ((entry = readdir(folder)))
	{
		tabFichierDossier[files] = entry->d_name;
		files++;
	}

	closedir(folder);

	int i = 0;
	while (i < files)
	{
		if (strcmp(tabFichierDossier[i], nomFichier) == 0)
		{
			printf("fichier deja existant\n");
			return -1;
		}
		i++;
	}

	int returnCode;
	int index;

	char *emplacementFichier = malloc(sizeof(char) * 40);
	strcpy(emplacementFichier, "./fichiers_serveur/");
	strcat(emplacementFichier, nomFichier);
	FILE *stream = fopen(emplacementFichier, "w");
	if (stream == NULL)
	{
		fprintf(stderr, "Cannot open file for writing\n");
		exit(-1);
	}

	if (tailleFichier != fwrite(buffer, sizeof(char), tailleFichier, stream))
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
		free(emplacementFichier);
		ecritureFichier(nomFichier, buffer, tailleFichier);
	}
	else
	{
		free(emplacementFichier);
	}
	return 0;
}

/**
 * @brief Fonction principale pour le thread gérant la copie de fichiers.
 *
 * @param clientIndex numéro du client qui envoie le fichier
 */
void *copieFichierThread(void *clientIndex)
{
	int i = (long)clientIndex;

	// Acceptons une connexion
	struct sockaddr_in aC;
	socklen_t lg = sizeof(struct sockaddr_in);
	tabClient[i].dSCFC = accept(dS_file, (struct sockaddr *)&aC, &lg);
	if (tabClient[i].dSCFC < 0)
	{
		perror("Problème lors de l'acceptation du client\n");
		exit(-1);
	}
	printf("Connécté\n");

	// Réception des informations du fichier
	int tailleFichier;
	char *nomFichier = (char *)malloc(sizeof(char) * 20);
	if (recv(tabClient[i].dSCFC, &tailleFichier, sizeof(int), 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
	receiving(tabClient[i].dSCFC, nomFichier, sizeof(char) * 20);

	// Début réception du fichier
	char *buffer = malloc(sizeof(char) * tailleFichier);

	receiving(tabClient[i].dSCFC, buffer, tailleFichier);

	if (ecritureFichier(nomFichier, buffer, tailleFichier) < 0)
	{
		sendingDM(tabClient[i].pseudo, "\nFichier déjà existant\nMerci de changer le nom du fichier\n");
	}


	sendingDM(tabClient[i].pseudo, "Téléchargement du fichier terminé\n");

	free(buffer);
	free(nomFichier);
	shutdown(tabClient[i].dSCFC, 2);
}

/**
 * @brief Fonction principale pour le thread gérant l'envoi de fichiers
 * à un client donné en paramètre.
 *
 * @param clientIndex numéro du client qui souhaite recevoir le fichier
 */
void *envoieFichierThread(void *clientIndex)
{
	int i = (long)clientIndex;
	char *nomFichier = malloc(sizeof(char) * 100);
	strcpy(nomFichier, tabClient[i].nomFichier);

	// DEBUT ENVOI FICHIER
	char *path = malloc(sizeof(char) * 120);
	strcpy(path, "./fichiers_serveur/");
	strcat(path, nomFichier);
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
	printf("Bytes fichier %d\n", tailleFichier);
	printf("Taille fichier %d\n", length);

	

	// Envoi de la taille du fichier, puis de son nom
	if (send(tabClient[i].dSCFC, &length, sizeof(int), 0) == -1)
	{
		perror("Erreur au send tailleFichier");
		exit(-1);
	}
	if (send(tabClient[i].dSCFC, nomFichier, strlen(nomFichier) + 1, 0) == -1)
	{
		perror("Erreur au send nomFichier");
		exit(-1);
	}

	if (send(tabClient[i].dSCFC, toutFichier, sizeof(char) * length, 0) == -1)
	{
		perror("Erreur au send toutFichier");
		exit(-1);
	}

	free(nomFichier);
	free(path);
	free(toutFichier);
	fclose(stream);
	shutdown(tabClient[i].dSCFC, 2);
}

/**
 * @brief Permet de JOIN les threads terminés.
 *
 * @param numclient indice du thread à join
 */
void endOfThread(int numclient)
{
	pthread_join(tabThread[numclient], 0);
	sem_post(&semaphoreThread);
}

/**
 * @brief Vérifie si un client souhaite utiliser une des commandes
 * disponibles.
 *
 * @param msg message du client à vérifier
 * @param pseudoSender pseudo du client qui envoie le message
 * @return 1 si le client utilise une commande, 0 sinon.
 */
int useOfCommand(char *msg, char *pseudoSender)
{
	char *strToken = strtok(msg, " ");
	if (strcmp(strToken, "/mp") == 0)
	{
		// Récupération du pseudo à qui envoyer le mp
		char *pseudoReceiver = (char *)malloc(sizeof(char) * 100);
		pseudoReceiver = strtok(NULL, " ");
		if (pseudoReceiver == NULL || verifPseudo(pseudoReceiver) == 0)
		{
			sendingDM(pseudoSender, "Pseudo érronné ou utilisation incorrecte de la commande /mp\n\"/aide\" pour plus d'indication\n");
			printf("Commande \"/mp\" mal utilisée\n");
			return 0;
		}
		char *msg = (char *)malloc(sizeof(char) * 115);
		msg = strtok(NULL, "");
		if (msg == NULL)
		{
			sendingDM(pseudoSender, "Message à envoyé vide\n\"/aide\" pour plus d'indication\n");
			printf("Commande \"/mp\" mal utilisée\n");
			return 0;
		}
		// Préparation du message à envoyer
		char *msgToSend = (char *)malloc(sizeof(char) * 115);
		strcpy(msgToSend, pseudoSender);
		strcat(msgToSend, " vous chuchotte : ");
		strcat(msgToSend, msg);

		// Envoi du message au destinataire
		printf("Envoi du message de %s au clients %s.\n", pseudoSender, pseudoReceiver);
		sendingDM(pseudoReceiver, msgToSend);
		free(msgToSend);
		return 1;
	}
	else if (strcmp(strToken, "/estConnecte") == 0)
	{
		// Récupération du pseudo
		char *pseudoToCheck = (char *)malloc(sizeof(char) * 100);
		pseudoToCheck = strtok(NULL, " ");
		pseudoToCheck = strtok(pseudoToCheck, "\n");

		// Préparation du message à envoyer
		char *msgToSend = (char *)malloc(sizeof(char) * 40);
		strcat(msgToSend, pseudoToCheck);

		if (verifPseudo(pseudoToCheck))
		{
			// Envoi du message au destinataire
			strcat(msgToSend, " est en ligne\n");
			sendingDM(pseudoSender, msgToSend);
		}
		else
		{
			// Envoi du message au destinataire
			strcat(msgToSend, " n'est pas en ligne\n");
			sendingDM(pseudoSender, msgToSend);
		}
		free(msgToSend);
		return 1;
	}
	else if (strcmp(strToken, "/aide\n") == 0)
	{
		// Envoie de l'aide au client, un message par ligne
		FILE *fichierCom = NULL;
		fichierCom = fopen("commande.txt", "r");
		fseek(fichierCom, 0, SEEK_END);
		int length = ftell(fichierCom);
		fseek(fichierCom, 0, SEEK_SET);

		if (fichierCom != NULL)
		{
			// char *chaine = (char *)malloc(100);
			char *toutFichier = (char *)malloc(length);
			fread(toutFichier, sizeof(char), length, fichierCom);
			// while (fgets(chaine, 100, fichierCom) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
			// {
			// 	strcat(toutFichier, chaine);
			// }
			sendingDM(pseudoSender, toutFichier);
			// free(chaine);
			free(toutFichier);
		}
		else
		{
			// On affiche un message d'erreur si le fichier n'a pas réussi a être ouvert
			printf("Impossible d\'ouvrir le fichier de commande pour l\'aide");
		}
		fclose(fichierCom);
		return 1;
	}
	else if (strcmp(strToken, "/enLigne\n") == 0)
	{
		int i = 0;
		while (i < MAX_CLIENT)
		{
			if (tabClient[i].isOccupied)
			{
				char *msgToSend = (char *)malloc(sizeof(char) * 30);
				strcpy(msgToSend, tabClient[i].pseudo);
				strcat(msgToSend, " est en ligne\n");
				sendingDM(pseudoSender, msgToSend);
				free(msgToSend);
			}
			i++;
		}
		return 1;
	}
	else if (strcmp(strToken, "/déposer\n") == 0)
	{
		long i = 0;
		while (i < MAX_CLIENT)
		{
			if (tabClient[i].isOccupied && strcmp(tabClient[i].pseudo, pseudoSender) == 0)
			{
				break;
			}
			i++;
		}
		if (i == MAX_CLIENT)
		{
			perror("Pseudo pas trouvé");
			exit(-1);
		}

		pthread_t copieFichier;

		if (pthread_create(&copieFichier, NULL, copieFichierThread, (void *)i) == -1)
		{
			perror("Erreur thread create");
		}

		return 1;
	}
	else if (strcmp(strToken, "/télécharger\n") == 0)
	{
		long i = 0;
		while (i < MAX_CLIENT)
		{
			if (tabClient[i].isOccupied && strcmp(tabClient[i].pseudo, pseudoSender) == 0)
			{
				break;
			}
			i++;
		}
		if (i == MAX_CLIENT)
		{
			perror("Pseudo pas trouvé");
			exit(-1);
		}

		// Acceptons une connexion
		struct sockaddr_in aC;
		socklen_t lg = sizeof(struct sockaddr_in);
		tabClient[i].dSCFC = accept(dS_file, (struct sockaddr *)&aC, &lg);
		if (tabClient[i].dSCFC < 0)
		{
			perror("Problème lors de l'acceptation du client\n");
			exit(-1);
		}

		char *afficheFichiers = malloc(sizeof(char) * 200);

		DIR *folder;
		struct dirent *entry;
		int files = 0;
		char numFile[10];

		folder = opendir("fichiers_serveur");
		if (folder == NULL)
		{
			perror("Unable to read directory");
			exit(EXIT_FAILURE);
		}

		entry = readdir(folder);
		entry = readdir(folder);
		strcpy(afficheFichiers, "Liste des fichiers disponibles :\n");
		while ((entry = readdir(folder)))
		{
			sprintf(numFile, "%d", files);
			strcat(afficheFichiers, "File ");
			strcat(afficheFichiers, numFile);
			strcat(afficheFichiers, ": ");
			strcat(afficheFichiers, entry->d_name);
			strcat(afficheFichiers, "\n");
			files++;
		}
		closedir(folder);

		if (send(tabClient[i].dSCFC, afficheFichiers, strlen(afficheFichiers) + 1, 0) == -1)
		{
			perror("Erreur à l'envoi du mp");
			exit(-1);
		}

		int numFichier;
		if (recv(tabClient[i].dSCFC, &numFichier, sizeof(int), 0) == -1)
		{
			perror("Erreur au recv");
			exit(-1);
		}

		folder = opendir("fichiers_serveur");
		if (folder == NULL)
		{
			perror("Unable to read directory");
			exit(EXIT_FAILURE);
		}

		entry = readdir(folder);
		entry = readdir(folder);
		int j = 0;
		while (j <= numFichier)
		{
			entry = readdir(folder);
			j++;
		}
		char *nomFichier = entry->d_name;
		closedir(folder);

		strcpy(tabClient[i].nomFichier, nomFichier);

		pthread_t envoieFichier;

		if (pthread_create(&envoieFichier, NULL, envoieFichierThread, (void *)(long)i) == -1)
		{
			perror("Erreur thread create");
		}
		free(afficheFichiers);
		return 1;
	}

	return 0;
}

/**
 * @brief Fonction principale de communication entre un
 * client et le serveur.
 *
 * @param clientParam numéro du client en question
 */
void *communication(void *clientParam)
{
	int isEnd = 0;
	int numClient = (long)clientParam;
	char *pseudoSender = tabClient[numClient].pseudo;

	while (!isEnd)
	{
		// Réception du message
		char *msgReceived = (char *)malloc(sizeof(char) * 100);
		receiving(tabClient[numClient].dSC, msgReceived, sizeof(char) * 100);
		printf("\nMessage recu: %s \n", msgReceived);

		// On verifie si le client veut terminer la communication
		isEnd = endOfCommunication(msgReceived);

		// On vérifie si le client utilise une des commandes
		char *msgToVerif = (char *)malloc(sizeof(char) * strlen(msgReceived));
		strcpy(msgToVerif, msgReceived);
		if (useOfCommand(msgToVerif, pseudoSender))
		{
			free(msgReceived);
			continue;
		}

		// Ajout du pseudo de l'expéditeur devant le message à envoyer
		char *msgToSend = (char *)malloc(sizeof(char) * 115);
		strcpy(msgToSend, pseudoSender);
		strcat(msgToSend, " : ");
		strcat(msgToSend, msgReceived);
		free(msgReceived);

		// Envoi du message aux autres clients
		printf("Envoi du message aux %ld clients. \n", nbClient - 1);
		sending(tabClient[numClient].dSC, msgToSend);
		free(msgToSend);
	}

	// Fermeture du socket client
	pthread_mutex_lock(&mutex);
	nbClient = nbClient - 1;
	tabClient[numClient].isOccupied = 0;
	pthread_mutex_unlock(&mutex);
	shutdown(tabClient[numClient].dSC, 2);

	// On relache le sémaphore pour les clients en attente
	sem_post(&semaphore);

	// On incrémente le sémaphore des threads
	sem_wait(&semaphoreThread);
	endOfThread(numClient);

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
    printf("\nFin du serveur\n");
    if (dS != 0)
    {
        sending(dS, "LE SERVEUR S'EST MOMENTANEMENT ARRETE, DECONNEXION...\n");
        int i = 0;
        while (i < MAX_CLIENT)
        {
            if (tabClient[i].isOccupied)
            {
                endOfThread(i);
            }
            i += 1;
        }
        shutdown(dS, 2);
        sem_destroy(&semaphore);
        sem_destroy(&semaphoreThread);
		pthread_mutex_destroy(&mutex);
    }
    exit(1);
}

/*
 * _____________________ MAIN _____________________
 */
// argv[1] = port
int main(int argc, char *argv[])
{
	// Verification du nombre de paramètres
	if (argc < 2)
	{
		perror("Erreur : Lancez avec ./serveur [votre_port] ");
		exit(-1);
	}
	printf("Début programme\n");

	// Fin avec Ctrl + C
    signal(SIGINT, sigintHandler);

	// Création de la socket
	dS_file = socket(PF_INET, SOCK_STREAM, 0);
	if (dS_file < 0)
	{
		perror("[Fichier] Problème de création de socket serveur");
		exit(-1);
	}
	printf("Socket [Fichier] Créé\n");

	// Nommage de la socket
	struct sockaddr_in ad_file;
	ad_file.sin_family = AF_INET;
	ad_file.sin_addr.s_addr = INADDR_ANY;
	ad_file.sin_port = htons(3001);
	if (bind(dS_file, (struct sockaddr *)&ad_file, sizeof(ad_file)) < 0)
	{
		perror("[Fichier] Erreur lors du nommage de la socket");
		exit(-1);
	}
	printf("[Fichier] Socket nommée\n");

	// Passage de la socket en mode écoute
	if (listen(dS_file, 7) < 0)
	{
		perror("[Fichier] Problème au niveau du listen");
		exit(-1);
	}
	printf("[Fichier] Mode écoute\n");

	// Création de la socket
	dS = socket(PF_INET, SOCK_STREAM, 0);
	if (dS < 0)
	{
		perror("Problème de création de socket serveur");
		exit(-1);
	}
	printf("Socket Créé\n");

	// Nommage de la socket
	struct sockaddr_in ad;
	ad.sin_family = AF_INET;
	ad.sin_addr.s_addr = INADDR_ANY;
	ad.sin_port = htons(atoi(argv[1]));
	if (bind(dS, (struct sockaddr *)&ad, sizeof(ad)) < 0)
	{
		perror("Erreur lors du nommage de la socket");
		exit(-1);
	}
	printf("Socket nommée\n");

	// Initialisation du sémaphore pour gérer les clients
	sem_init(&semaphore, PTHREAD_PROCESS_SHARED, MAX_CLIENT);

	// Initialisation du sémaphore pour gérer les threads
	sem_init(&semaphoreThread, PTHREAD_PROCESS_SHARED, 1);

	// Passage de la socket en mode écoute
	if (listen(dS, 7) < 0)
	{
		perror("Problème au niveau du listen");
		exit(-1);
	}
	printf("Mode écoute\n");

	while (1)
	{
		int dSC;
		// Vérifions si on peut accepter un client
		// On attends la disponibilité du sémaphore
		sem_wait(&semaphore);

		// Acceptons une connexion
		struct sockaddr_in aC;
		socklen_t lg = sizeof(struct sockaddr_in);
		dSC = accept(dS, (struct sockaddr *)&aC, &lg);
		if (dSC < 0)
		{
			perror("Problème lors de l'acceptation du client\n");
			exit(-1);
		}
		printf("Client %ld connecté\n", nbClient);

		// Réception du pseudo
		char *pseudo = (char *)malloc(sizeof(char) * 100);
		receiving(dSC, pseudo, sizeof(char) * 12);
		pseudo = strtok(pseudo, "\n");

		while (verifPseudo(pseudo))
		{
			send(dSC, "Pseudo déjà existant\n", strlen("Pseudo déjà existant\n"), 0);
			receiving(dSC, pseudo, sizeof(char) * 12);
			pseudo = strtok(pseudo, "\n");
		}

		// Enregistrement du client
		long numClient = giveNumClient();
		pthread_mutex_lock(&mutex);
		tabClient[numClient].isOccupied = 1;
		tabClient[numClient].dSC = dSC;
		tabClient[numClient].pseudo = (char *)malloc(sizeof(char) * 12);
		strcpy(tabClient[numClient].pseudo, pseudo);
		pthread_mutex_unlock(&mutex);

		// On envoie un message pour dire au client qu'il est bien connecté
		char *repServ = (char *)malloc(sizeof(char) * 100);
		repServ = "Entrer /aide pour avoir la liste des commandes disponibles\n";
		sendingDM(pseudo, repServ);
		// On vérifie que ce n'est pas le pseudo par défaut
		if (strcmp(pseudo, "FinClient") != 0)
		{
			// On envoie un message pour avertir les autres clients de l'arrivée du nouveau client
			strcat(pseudo, " a rejoint la communication\n");
			sending(dSC, pseudo);
		}
		free(pseudo);
		//_____________________ Communication _____________________
		if (pthread_create(&tabThread[numClient], NULL, communication, (void *)numClient) == -1)
		{
			perror("Erreur thread create");
		}

		// On a un client en plus sur le serveur, on incrémente
		pthread_mutex_lock(&mutex);
		nbClient += 1;
		pthread_mutex_unlock(&mutex);
		printf("Clients connectés : %ld\n", nbClient);
	}
	// ############  	N'arrive jamais  	####################
	shutdown(dS, 2);
	sem_destroy(&semaphore);
	sem_destroy(&semaphoreThread);
	pthread_mutex_destroy(&mutex);
	printf("Fin du programme\n");
	// #########################################################
}