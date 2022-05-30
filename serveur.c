/*
// 
SALON ENREGISTRE DANS UN DOC AVEC ID NOM DESC ? (mdp pour modifier et supp un salon ?)
CHANGER GIVE NUMSALON
CHANGER /SALON
CREER CREER SALON, MODIFIER ET SUPPRIMER
Gestion du ctrl +c (include + main + fonction sigintHandler)
Structure salon + variables + sem/mutex
Include time
Veuillez trouver le diagramme de Séquence sur ce lien :
//www.plantuml.com/plantuml/png/ROz12i9034NtESLVwg8Nc8KKz0Jg1KARnS0qaJGLZ-Esv-Z57B0LX488_9T7Gjens6CQ2d4NvZYNB1fhk8a_PNBwGZIdZI3X8WDhBwZLcQgyiYbjup_pxfn31j7ODzVj2TTbVfYEWkMDmkZtBd0977uH2MhQy1JcULncEIOYqPxQskfJ7m00
*/
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

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
	int idSalon;
	char *pseudo;
	long dSCFC;
	char nomFichier[100];
};

/**
 *  @brief Définition d'une structure Salon pour regrouper toutes les informations d'un salon
 * *
 * @param idSalon id du salon
 * @param isOccupiedSalon 1 si le Salon existe ; 0 sinon
 * @param nom Appellation du Salon, donné à la création
 * @param description Description du salon, donné à la création
 * @param nbPlace nombre de place que peut accepter le salon, donné à la création
 */
typedef struct Salon Salon;
struct Salon
{
	int idSalon;
	int isOccupiedSalon;
	char *nom;
	char *description;
	int nbPlace;
};

/**
 * - MAX_CLIENT = nombre maximum de clients acceptés sur le serveur
 * - tabClient = tableau répertoriant les clients connectés
 * - tabThread = tableau des threads associés au traitement de chaque client
 * - tabSalon = tableau répertoriant les salons existants
 * - nbClients = nombre de clients actuellement connectés
 * - dS_file = socket de connexion pour le transfert de fichiers
 * - dS = socket de connexion entre les clients et le serveur
 * - semaphore = sémaphore pour gérer le nombre de clients
 * - semaphoreThread = sémpahore pour gérer les threads
 * - mutex = mutex pour la modification de tabClient[]
 * - mutexSalon = mutex pour lla modification de tabSalon[]
 */

#define MAX_CLIENT 3
#define MAX_SALON 3
Client tabClient[MAX_CLIENT];
pthread_t tabThread[MAX_CLIENT];
Salon tabSalon[MAX_SALON];
long nbClient = 0;
int dS_file;
int dS;
int portServeur;
sem_t semaphore;
sem_t semaphoreThread;
pthread_mutex_t mutex;
pthread_mutex_t mutexSalon;

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

/* A CHANGER
 * Fonctions pour gérer les indices du tableaux de salons
 * Retour : un entier, indice du premier emplacement disponible
 *          -1 si tout les emplacements sont occupés.
 */
int giveNumSalon()
{
    int i = 0;
    while (i < MAX_SALON)
    {
        if (!tabSalon[i].isOccupiedSalon)
        {
            return i;
        }
        i += 1;
    }
    return(-1);
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
 * @brief Fonctions pour envoyer à un utilisateur tout les salons disponible
 *
 * @param pseudo pseudo à qui envoyer l'information
 * @return une chaine ;
 *         composée de la description des salons disponibles (id, nom, nbPlace et description)
 */
void afficheSalon(char *pseudoSender)
{
    for (int i = 0; i < MAX_SALON; i++)
    {
        char j[MAX_SALON];
        printf(" i = %d\n", i);
        printf("isOccupied = %d\n", tabSalon[i].isOccupiedSalon);
        if (tabSalon[i].isOccupiedSalon)
        {
            sprintf(j, "%d", i);
            char *rep = malloc(sizeof(char) * 300);
            strcpy(rep, "-------------------------------------\n");
            strcat(rep, j);
            strcat(rep, ": ");
            strcat(rep, "Nom: ");
            strcat(rep, tabSalon[i].nom);
            strcat(rep, "\n");
            strcat(rep, "Description: ");
            strcat(rep, tabSalon[i].description);
            strcat(rep, "\n\n");
            sendingDM(pseudoSender, rep);
            sleep(0.3);
            free(rep);
        }
    }
    sendingDM(pseudoSender, "-------------------------------------\n");
}

/**
 * @brief Fonctions pour vérifier si le salon existe pour un idSalon.
 *
 * @param numSalon id de salon à vérifier si existant
 * @return un integer ;
 *         1 si le salon existe,
 *         0 si le salon n'existe pas.
 */
int salonExiste(int numSalon)
{
	if(numSalon>MAX_SALON || numSalon < 0 ){
        return 0;
    } else {
        printf("dans exist");
        return tabSalon[numSalon].isOccupiedSalon;
    }
}
/**
 * @brief Fonctions pour vérifier si un salon peut accepter un nouvel utilisateur.
 *
 * @param numSalon id de salon à vérifier si acceptant
 * @return un integer ;
 *         1 si le salon a de la place,
 *         0 si le salon n'en a pas.
 */
int salonAcceptNewUser(int numSalon)
{
    int nbPlace = 0;
    for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].isOccupied && tabClient[i].idSalon == numSalon)
		{
			nbPlace++;
		}
	}
    if(nbPlace<tabSalon[numSalon].nbPlace){
        return 1;
    } else {
        return 0;
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
 * @brief Envoie un message à toutes les sockets présentes dans le tableau des clients
 * et teste que tout se passe bien.
 *
 * @param dS expéditeur du message
 * @param msg message à envoyer
 */
void sending(int dS, char *msg, int id)
{
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].isOccupied && dS != tabClient[i].dSC && id == tabClient[i].idSalon)
		{
			if (send(tabClient[i].dSC, msg, strlen(msg) + 1, 0) == -1)
			{
				perror("Erreur au send");
				exit(-1);
			}
		}
	}
}

void sendingAll(char *msg) {
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].isOccupied)
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

	char *emplacementFichier = malloc(sizeof(char) * 120);
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
	char *nomFichier = (char *)malloc(sizeof(char) * 100);
	if (recv(tabClient[i].dSCFC, &tailleFichier, sizeof(int), 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
	receiving(tabClient[i].dSCFC, nomFichier, sizeof(char) * 100);

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
			return 1;
		}
		char *msg = (char *)malloc(sizeof(char) * 115);
		msg = strtok(NULL, "");
		if (msg == NULL)
		{
			sendingDM(pseudoSender, "Message à envoyé vide\n\"/aide\" pour plus d'indication\n");
			printf("Commande \"/mp\" mal utilisée\n");
			return 1;
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
				char *msgToSend = (char *)malloc(sizeof(char) * 120);
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
	}else if (strcmp(strToken, "/télécharger\n") == 0)
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
	else if (strcmp(strToken, "/créer") == 0)
	{
		pthread_mutex_lock(&mutexSalon);
		int numSalon = giveNumSalon();
		if (numSalon == -1)
		{
			sendingDM(pseudoSender, "Le maximum de salon est atteint, vous ne pouvez pas en créer un autre pour le moment\n");
		}
		else
		{
			char *nomSalon = strtok(NULL, " ");
			int nbPlaces = atoi(strtok(NULL, " "));
			char *description = strtok(NULL, "");

			// TODO: verifier les infos (nbPlace bien un int)
			printf("nom : %s\n", nomSalon);
			printf("nbPlaces : %d\n", nbPlaces);
			printf("%s\n", description);
			tabSalon[numSalon].idSalon = numSalon;
			tabSalon[numSalon].nom = nomSalon;
			tabSalon[numSalon].nbPlace = nbPlaces;
			tabSalon[numSalon].description = description;
			tabSalon[numSalon].isOccupiedSalon = 1;
			pthread_mutex_unlock(&mutexSalon);
			// TODO: ecrire dans un fichier les infos du salon
			sendingDM(pseudoSender, "Le salon a bien été créé\n");
		}
		return 1;
	}
	else if (strcmp(strToken, "/liste\n") == 0)
	{
		for (int i = 0; i < MAX_SALON; i++)
		{
			char j[MAX_SALON];
			printf(" i = %d\n", i);
			printf("isOccupied = %d\n", tabSalon[i].isOccupiedSalon);
			if (tabSalon[i].isOccupiedSalon)
			{
				sprintf(j, "%d", i);
				char *rep = malloc(sizeof(char) * 300);
				strcpy(rep, "-------------------------------------\n");
				strcat(rep, j);
				strcat(rep, ": ");
				strcat(rep, "Nom: ");
				strcat(rep, tabSalon[i].nom);
				strcat(rep, "\n");
				strcat(rep, "Description: ");
				strcat(rep, tabSalon[i].description);
				strcat(rep, "\n\n");
				sendingDM(pseudoSender, rep);
				sleep(0.3);
				free(rep);
			}
		}
		sendingDM(pseudoSender, "-------------------------------------\n");
		return 1;
	}
	else if (strcmp(strToken, "/suppression") == 0)
	{
		char *nomSalon = malloc(sizeof(char) * 30);
		nomSalon = strtok(NULL, " ");

		int i = 0;
		while (i < MAX_SALON)
		{
			if (tabSalon[i].isOccupiedSalon && strcmp(tabSalon[i].nom, nomSalon) == 0)
			{
				break;
			}
		}
		if (i == MAX_SALON)
		{
			sendingDM(pseudoSender, "Le nom du salon n'existe pas\n");
		}
		else
		{
			free(tabSalon[i].nom);
			free(tabSalon[i].description);
			tabSalon[i].isOccupiedSalon = 0;
			sendingDM(pseudoSender, "Le salon a été supprimé\n");
		}

		return 1;
	}
    else if (strcmp(strToken, "/connexionSalon\n") == 0)
    {
		int numSalon; // num salon
        int i = 0;
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
        afficheSalon(pseudoSender);
        sleep(0.2);
		sendingDM(pseudoSender, "Rentrez le numéro de salon souhaité. Si vous souhaitez annuler, tapez -1\n");
        
		if (recv(tabClient[i].dSC, &numSalon, sizeof(int), 0) == -1)
        {
            perror("Erreur au recv");
            exit(-1);
        }
        printf("prems %d", numSalon);
        while((numSalon == -1) || !salonExiste((int)numSalon)){
            sendingDM(pseudoSender, "id de salon incorrect, veuillez en rentrer un autre. Si vous souhaitez annuler, tapez -1\n");
            if (recv(tabClient[i].dSC, &numSalon, sizeof(int), 0) == -1)
            {
                perror("Erreur au recv");
                exit(-1);
            }
            sleep(0.1);
            sendingDM(pseudoSender, numSalon);
            printf("while %d", numSalon);
        }
        if(numSalon == -1){
            sendingDM(pseudoSender, "Annulation du changement de salon.");
        }
        else if(salonAcceptNewUser(numSalon)){
            tabClient[i].idSalon = numSalon;
        } else {
            sendingDM(pseudoSender, "Ce salon comporte trop de membres, veuillez réessayer plus tard.");
        }
        
        return 1;
    }
    else if (strToken[0] == '/')
	{
		sendingDM(pseudoSender, "Faites \"/aide\" pour avoir accès aux commandes disponibles et leur fonctionnement\n");
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
	int numClient = (long)clientParam;

	// Réception du pseudo
	char *pseudo = (char *)malloc(sizeof(char) * 100);
	receiving(tabClient[numClient].dSC, pseudo, sizeof(char) * 12);
	pseudo = strtok(pseudo, "\n");

	while (verifPseudo(pseudo))
	{
		send(tabClient[numClient].dSC, "Pseudo déjà existant\n", strlen("Pseudo déjà existant\n"), 0);
		receiving(tabClient[numClient].dSC, pseudo, sizeof(char) * 12);
		pseudo = strtok(pseudo, "\n");
	}
	
	tabClient[numClient].pseudo = (char *)malloc(sizeof(char) * 12);
	strcpy(tabClient[numClient].pseudo, pseudo);
	tabClient[numClient].idSalon = 0;

	// On envoie un message pour dire au client qu'il est bien connecté
	char *repServ = (char *)malloc(sizeof(char) * 100);
	repServ = "Entrer /aide pour avoir la liste des commandes disponibles\n";
	sendingDM(pseudo, repServ);
	// On vérifie que ce n'est pas le pseudo par défaut
	if (strcmp(pseudo, "FinClient") != 0)
	{
		// On envoie un message pour avertir les autres clients de l'arrivée du nouveau client
		strcat(pseudo, " a rejoint la communication\n");
		sending(tabClient[numClient].dSC, pseudo, 0);
	}

	// On a un client en plus sur le serveur, on incrémente
	pthread_mutex_lock(&mutex);
	nbClient += 1;
	pthread_mutex_unlock(&mutex);
	printf("Clients connectés : %ld\n", nbClient);

	int isEnd = 0;
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
		sending(tabClient[numClient].dSC, msgToSend, tabClient[numClient].idSalon);
		free(msgToSend);
	}

	// Fermeture du socket client
	pthread_mutex_lock(&mutex);
	nbClient = nbClient - 1;
	tabClient[numClient].isOccupied = 0;
	free(tabClient[numClient].pseudo);
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
		sendingAll("LE SERVEUR S'EST MOMENTANEMENT ARRETE, DECONNEXION...\n");
		sendingAll("Tout ce message est le code secret pour désactiver les clients");

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

	portServeur = atoi(argv[1]);

	// Fin avec Ctrl + C
	signal(SIGINT, sigintHandler);


	tabSalon[0].idSalon = 0;
	tabSalon[0].isOccupiedSalon = 1;
	tabSalon[0].nom = malloc(sizeof(char) * 40);
	tabSalon[0].nom = "Chat générale";
	tabSalon[0].description = "Salon général par défaut";

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
	ad_file.sin_port = htons(portServeur + 1);
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
		// Vérifions si on peut accepter un client
		// On attends la disponibilité du sémaphore
		sem_wait(&semaphore);

		// Acceptons une connexion
		struct sockaddr_in aC;
		socklen_t lg = sizeof(struct sockaddr_in);
		int dSC = accept(dS, (struct sockaddr *)&aC, &lg);
		if (dSC < 0)
		{
			perror("Problème lors de l'acceptation du client\n");
			exit(-1);
		}
		printf("Client %ld connecté\n", nbClient);

		// Enregistrement du client
		pthread_mutex_lock(&mutex);
		long numClient = giveNumClient();
		tabClient[numClient].isOccupied = 1;
		tabClient[numClient].dSC = dSC;
		tabClient[numClient].pseudo = " ";
		pthread_mutex_unlock(&mutex);

		//_____________________ Communication _____________________
		if (pthread_create(&tabThread[numClient], NULL, communication, (void *)numClient) == -1)
		{
			perror("Erreur thread create");
		}
	}
	// ############  	N'arrive jamais  	####################
	shutdown(dS, 2);
	sem_destroy(&semaphore);
	sem_destroy(&semaphoreThread);
	pthread_mutex_destroy(&mutex);
	printf("Fin du programme\n");
	// #########################################################
}