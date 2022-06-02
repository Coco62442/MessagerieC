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
 * @param estOccupe 1 si le Client est connecté au serveur ; 0 sinon
 * @param dSC Socket de transmission des messages classiques au Client
 * @param pseudo Appellation que le Client rentre à sa première connexion
 * @param dSCFC Socket de transfert des fichiers
 * @param nomFichier Nomination du fichier choisi par le client pour le transfert
 */
typedef struct Client Client;
struct Client
{
	int estOccupe;
	long dSC;
	int idSalon;
	char *pseudo;
	long dSCFC;
	char nomFichier[100];
};

/**
 *  @brief Définition d'une structure Salon pour regrouper toutes les informations d'un salon.
 *
 * @param idSalon Identifiant du salon
 * @param estOccupe 1 si le salon existe ; 0 sinon
 * @param nom Appellation du salon, donné à la création (max 20)
 * @param description Description du salon, donné à la création (max 200)
 * @param nbPlace Nombre de place que peut accepter le salon, donné à la création
 */
typedef struct Salon Salon;
struct Salon
{
	int idSalon;
	int estOccupe;
	char *nom;
	char *description;
	int nbPlace;
};

/**
 * - MAX_CLIENT = nombre maximum de clients acceptés sur le serveur
 * - MAX_SALON = nombre maximum de salons sur le serveur
 * - TAILLE_PSEUDO = taille maximum du pseudo
 * - TAILLE_DESCRIPTION = taille maximum de la description du salon
 * - TAILLE_NOM_SALON = taille maximum du nom du salon
 * - DOSSIER_SERVEUR = nom du dossier où sont stockés les fichiers
 * - TAILLE_NOM_FICHIER = taille maximum du nom du fichier
 * - TAILLE_MESSAGE = taille maximum d'un message
 */
#define MAX_CLIENT 3
#define MAX_SALON 7
#define TAILLE_PSEUDO 20
#define TAILLE_DESCRIPTION 200
#define TAILLE_NOM_SALON 20
#define DOSSIER_SERVEUR "fichiers_serveur"
#define TAILLE_NOM_FICHIER 100
#define TAILLE_MESSAGE 500

/**
 * - tabClient = tableau répertoriant les clients connectés
 * - tabThread = tableau des threads associés au traitement de chaque client
 * - tabSalon = tableau répertoriant les salons existants
 * - nbClients = nombre de clients actuellement connectés
 * - dS_fichier = socket de connexion pour le transfert de fichiers
 * - dS = socket de connexion entre les clients et le serveur
 * - portServeur = port sur lequel le serveur est exécuté
 * - semaphoreNbClients = sémaphore pour gérer le nombre de clients
 * - semaphoreThread = sémpahore pour gérer les threads
 * - mutexTabClient = mutexTabClient pour la modification de tabClient[]
 * - mutexSalon = mutexTabClient pour la modification de tabSalon[]
 */
Client tabClient[MAX_CLIENT];
pthread_t tabThread[MAX_CLIENT];
Salon tabSalon[MAX_SALON];
long nbClient = 0;
int dS_fichier;
int dS;
int portServeur;
sem_t semaphoreNbClients;
sem_t semaphoreThread;
pthread_mutex_t mutexTabClient;
pthread_mutex_t mutexSalon;

// Déclaration des fonctions
int donnerNumClient();
int donnerNumSalon();
int verifPseudo(char *pseudo);
void afficheSalon(char *pseudoEnvoyeur);
int salonExiste(int numSalon);
int accepteNouvelUtilisateur(int numSalon);
long pseudoTodSC(char *pseudo);
void envoi(int dS, char *msg, int id);
void envoiATous(char *msg);
void envoiPrive(char *pseudoRecepteur, char *msg);
void reception(int dS, char *rep, ssize_t size);
int finDeCommunication(char *msg);
int ecritureFichier(char *nomFichier, char *buffer, int tailleFichier);
void *copieFichierThread(void *clientIndex);
void *envoieFichierThread(void *clientIndex);
int nbChiffreDansNombre(int nombre);
void ecritureSalon();
void endOfThread(int numclient);
int utilisationCommande(char *msg, char *pseudoEnvoyeur);
void *communication(void *clientParam);
void sigintHandler(int sig_num);

/**
 * @brief Fonction pour gérer les indices du tableau de clients.
 *
 * @return un entier, indice du premier emplacement disponible ;
 *         -1 si tout les emplacements sont occupés.
 */
int donnerNumClient()
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (!tabClient[i].estOccupe)
		{
			return i;
		}
		i += 1;
	}
	return -1;
}

/**
 * @brief Fonction pour gérer les indices du tableau de salons.
 *
 * @return un entier, indice du premier emplacement disponible ;
 *         -1 si tout les emplacements sont occupés.
 */
int donnerNumSalon()
{
	int i = 0;
	while (i < MAX_SALON)
	{
		if (!tabSalon[i].estOccupe)
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
		if (tabClient[i].estOccupe && strcmp(pseudo, tabClient[i].pseudo) == 0)
		{
			return 1;
		}
		i++;
	}
	return 0;
}

/**
 * @brief Fonction pour envoyer à un utilisateur tous les salons disponibles.
 *
 * @param pseudo pseudo à qui envoyer l'information
 * @return une chaine ;
 *         composée de la description des salons disponibles (id, nom et description)
 */
void afficheSalon(char *pseudoEnvoyeur)
{
	char *chaineAffiche = malloc(sizeof(char) * (TAILLE_DESCRIPTION + TAILLE_NOM_SALON + 50) * 4); // Tous les 5 salons envoie de la chaine concaténée
	int place;
	int compteur = 0;
	char numSalonChar[MAX_SALON];
	for (int i = 0; i < MAX_SALON; i++)
	{
		if (tabSalon[i].estOccupe)
		{
			compteur++;
			place = 0;
			int intId = nbChiffreDansNombre(tabSalon[i].idSalon);
			place += intId;
			place += strlen(tabSalon[i].nom);
			place += strlen(tabSalon[i].description);
			place += 50; // 22+3+6+2+14+3 pour les caractères visuels

			char *rep = malloc(sizeof(char) * place);

			sprintf(numSalonChar, "%d", i);
			strcpy(rep, "--------------------\n"); // 22
			strcat(rep, numSalonChar);
			strcat(rep, ": ");
			strcat(rep, "Nom: ");
			strcat(rep, tabSalon[i].nom);
			strcat(rep, "\n");
			strcat(rep, "Description: ");
			strcat(rep, tabSalon[i].description);
			strcat(rep, "\n");

			strcat(chaineAffiche, rep);

			free(rep);
		}
		if (compteur == 3)
		{
			envoiPrive(pseudoEnvoyeur, chaineAffiche);
			strcpy(chaineAffiche, "");
			compteur = 0;
		}
	}
	if (compteur != 0)
	{
		envoiPrive(pseudoEnvoyeur, chaineAffiche);
	}
	free(chaineAffiche);
}

/**
 * @brief Fonctions pour vérifier si le salon existe pour un idSalon.
 *
 * @param numSalon id du salon à vérifier
 * @return un entier ;
 *         1 si le salon existe,
 *         0 si le salon n'existe pas.
 */
int salonExiste(int numSalon)
{
	if (numSalon > MAX_SALON || numSalon < 0)
	{
		return 0;
	}
	else
	{
		return tabSalon[numSalon].estOccupe;
	}
}

/**
 * @brief Fonction pour vérifier si un salon peut accepter un nouvel utilisateur.
 *
 * @param numSalon id de salon à vérifier
 * @return un entier ;
 *         1 si le salon a de la place,
 *         0 si le salon n'en a pas.
 */
int accepteNouvelUtilisateur(int numSalon)
{
	int nbPlace = 0;
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		if (tabClient[i].estOccupe && tabClient[i].idSalon == numSalon)
		{
			nbPlace++;
		}
	}
	if (nbPlace < tabSalon[numSalon].nbPlace)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * @brief Fonction pour récupérer l'index dans tabClient selon un pseudo donné.
 *
 * @param pseudo pseudo pour lequel on cherche l'index du tableau
 * @return index du client nommé [pseudo] ;
 *         -1 si le pseudo n'existe pas.
 */
long pseudoToInt(char *pseudo)
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (tabClient[i].estOccupe && strcmp(tabClient[i].pseudo, pseudo) == 0)
		{
			return i;
		}
		i++;
	}
	return -1;
}

/**
 * @brief Envoie un message à toutes les sockets présentes dans le tableau des clients pour un même idSalon
 * et teste que tout se passe bien.
 *
 * @param dS expéditeur du message
 * @param msg message à envoyer
 * @param idSalon id du salon sur lequel envoyé le message
 */
void envoi(int dS, char *msg, int idSalon)
{
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].estOccupe && dS != tabClient[i].dSC && idSalon == tabClient[i].idSalon && strcmp(tabClient[i].pseudo, " ") != 0)
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
 * @brief Envoie un message à toutes les sockets présentes dans le tableau des clients
 * et teste que tout se passe bien.
 *
 * @param msg message à envoyer
 */
void envoiATous(char *msg)
{
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].estOccupe)
		{
			if (send(tabClient[i].dSC, msg, strlen(msg) + 1, 0) == -1)
			{
				perror("Erreur à l'envoi à tout le monde");
				exit(-1);
			}
		}
	}
}

/**
 * @brief Envoie un message en privé à un client en particulier
 * et teste que tout se passe bien.
 *
 * @param pseudoRecepteur destinataire du message
 * @param msg message à envoyer
 */
void envoiPrive(char *pseudoRecepteur, char *msg)
{
	int i = pseudoToInt(pseudoRecepteur);
	if (i == -1)
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
void reception(int dS, char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		perror("Erreur à la réception");
		exit(-1);
	}
}

/**
 * @brief Fonction pour vérifier si un client souhaite quitter la communication.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client veut quitter, 0 sinon.
 */
int finDeCommunication(char *msg)
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

	DIR *dossier;
	struct dirent *entry;
	int fichiers = 0;

	dossier = opendir(DOSSIER_SERVEUR);
	if (dossier == NULL)
	{
		perror("Impossible d'ouvrir le dossier");
		exit(EXIT_FAILURE);
	}

	while ((entry = readdir(dossier)))
	{
		tabFichierDossier[fichiers] = entry->d_name;
		fichiers++;
	}

	closedir(dossier);

	int i = 0;
	while (i < fichiers)
	{
		if (strcmp(tabFichierDossier[i], nomFichier) == 0)
		{
			printf("Fichier déjà existant\n");
			return -1;
		}
		i++;
	}

	int codeRetour;
	int index;

	char *emplacementFichier = malloc(sizeof(char) * (strlen(DOSSIER_SERVEUR) + 2 + strlen(nomFichier)));
	strcpy(emplacementFichier, DOSSIER_SERVEUR);
	strcat(emplacementFichier, "/");
	strcat(emplacementFichier, nomFichier);

	FILE *stream = fopen(emplacementFichier, "w");

	if (stream == NULL)
	{
		fprintf(stderr, "Impossible d'ouvrir le fichier pour écriture\n");
		exit(-1);
	}

	if (tailleFichier != fwrite(buffer, sizeof(char), tailleFichier, stream))
	{
		fprintf(stderr, "Impossible d'écrire un block dans le fichier\n");
		exit(EXIT_FAILURE);
	}

	fseek(stream, 0, SEEK_END);
	int longueur = ftell(stream);
	fseek(stream, 0, SEEK_SET);

	codeRetour = fclose(stream);
	if (codeRetour == EOF)
	{
		fprintf(stderr, "Cannot close file\n");
		exit(-1);
	}

	if (tailleFichier != longueur)
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
	tabClient[i].dSCFC = accept(dS_fichier, (struct sockaddr *)&aC, &lg);

	if (tabClient[i].dSCFC < 0)
	{
		perror("Problème lors de l'acceptation du client\n");
		exit(-1);
	}

	// Réception des informations du fichier
	int tailleFichier;
	char *nomFichier = (char *)malloc(sizeof(char) * TAILLE_NOM_FICHIER);
	if (recv(tabClient[i].dSCFC, &tailleFichier, sizeof(int), 0) == -1)
	{
		perror("Erreur à la réception de la taille du fichier");
		exit(-1);
	}
	reception(tabClient[i].dSCFC, nomFichier, sizeof(char) * TAILLE_NOM_FICHIER);

	// Début réception du fichier
	char *buffer = malloc(sizeof(char) * tailleFichier);

	reception(tabClient[i].dSCFC, buffer, tailleFichier);

	if (ecritureFichier(nomFichier, buffer, tailleFichier) < 0)
	{
		envoiPrive(tabClient[i].pseudo, "\nFichier déjà existant\nMerci de changer le nom du fichier\n");
	}

	envoiPrive(tabClient[i].pseudo, "Téléchargement du fichier terminé\n");

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
	char *nomFichier = malloc(sizeof(char) * TAILLE_NOM_FICHIER);
	strcpy(nomFichier, tabClient[i].nomFichier);

	// DEBUT ENVOI FICHIER
	char *chemin = malloc(sizeof(char) * (strlen(DOSSIER_SERVEUR) + 2 + strlen(nomFichier)));
	strcpy(chemin, DOSSIER_SERVEUR);
	strcat(chemin, "/");
	strcat(chemin, nomFichier);

	FILE *stream = fopen(chemin, "r");

	if (stream == NULL)
	{
		fprintf(stderr, "[ENVOI FICHIER] Impossible d'ouvrir le fichier en lecture\n");
		exit(-1);
	}

	fseek(stream, 0, SEEK_END);
	int longueur = ftell(stream);
	fseek(stream, 0, SEEK_SET);

	// Lecture et stockage pour envoi du fichier
	char *toutFichier = malloc(sizeof(char) * longueur);
	int tailleFichier = fread(toutFichier, sizeof(char), longueur, stream);

	// Envoi de la taille du fichier, puis de son nom
	if (send(tabClient[i].dSCFC, &longueur, sizeof(int), 0) == -1)
	{
		perror("Erreur à l'envoi tailleFichier");
		exit(-1);
	}
	if (send(tabClient[i].dSCFC, nomFichier, strlen(nomFichier) + 1, 0) == -1)
	{
		perror("Erreur à l'envoi nomFichier");
		exit(-1);
	}

	if (send(tabClient[i].dSCFC, toutFichier, sizeof(char) * longueur, 0) == -1)
	{
		perror("Erreur à l'envoi toutFichier");
		exit(-1);
	}

	free(nomFichier);
	free(chemin);
	free(toutFichier);
	fclose(stream);
	shutdown(tabClient[i].dSCFC, 2);
}

/**
 * @brief Permet de savoir la longueur d'un chiffre.
 *
 * @param nombre le nombre dont on souhaite connaitre la taille
 * @return le nombre de chiffre qui compose ce nombre.
 */
int nbChiffreDansNombre(int nombre)
{
	int nbChiffre = 0;
	while (nombre > 0)
	{
		nombre = nombre / 10;
		nbChiffre += 1;
	}
	return nbChiffre;
}

/**
 * @brief Enregistre le tableau de salon sous la forme d'un fichier txt
 *
 */
void ecritureSalon()
{
	FILE *stream = fopen("fichierSalon.txt", "w");

	if (stream == NULL)
	{
		fprintf(stderr, "Impossible d'ouvrir le fichier en écriture\n");
		exit(-1);
	}

	int place;

	for (int i = 1; i < MAX_SALON; i++)
	{
		if (tabSalon[i].estOccupe)
		{
			place = 0;

			int intId = nbChiffreDansNombre(tabSalon[i].idSalon);
			place += intId;

			place += strlen(tabSalon[i].nom);

			int intNbPlace = nbChiffreDansNombre(tabSalon[i].nbPlace);
			place += intNbPlace;

			place += strlen(tabSalon[i].description);

			place += 3; // Trois espaces

			// Variable définissant une ligne du fichier à écrire
			char *ligne = malloc(sizeof(char) * place);
			// Ecriture de l'id du salon
			char idSal[intId];
			sprintf(idSal, "%d", tabSalon[i].idSalon);
			strcpy(ligne, idSal);
			strcat(ligne, " ");
			// Ecriture du nom du salon
			strcat(ligne, tabSalon[i].nom);
			strcat(ligne, " ");
			// Ecriture de la place (nombre d'utilisateur max pour le salon)
			char nbSal[intNbPlace];
			sprintf(nbSal, "%d", tabSalon[i].nbPlace);
			strcat(ligne, nbSal);
			strcat(ligne, " ");
			// Ecriture de la description du salon
			strcat(ligne, tabSalon[i].description);
			strcat(ligne, "\n");

			//Ecriture dans le fichier de la ligne
			fwrite(ligne, sizeof(char), place, stream);
			free(ligne);
		}
	}
	fclose(stream);
}

/**
 * @brief Permet de join les threads terminés.
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
 * @param pseudoEnvoyeur pseudo du client qui envoie le message
 *
 * @return 1 si le client utilise une commande, 0 sinon.
 */
int utilisationCommande(char *msg, char *pseudoEnvoyeur)
{
	char *strToken = strtok(msg, " ");
	if (strcmp(strToken, "/mp") == 0)
	{
		// Récupération du pseudo à qui envoyer le mp
		char *pseudoRecepteur = (char *)malloc(sizeof(char) * TAILLE_PSEUDO);
		pseudoRecepteur = strtok(NULL, " ");

		if (pseudoRecepteur == NULL || verifPseudo(pseudoRecepteur) == 0)
		{
			envoiPrive(pseudoEnvoyeur, "Pseudo érronné ou utilisation incorrecte de la commande /mp\nFaites \"/aide\" pour plus d'informations\n");
			printf("Commande \"/mp\" mal utilisée\n");
			return 1;
		}

		char *msg = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		msg = strtok(NULL, "");

		if (msg == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Message à envoyé vide\nFaites \"/aide\" pour plus d'informations\n");
			printf("Commande \"/mp\" mal utilisée\n");
			return 1;
		}

		// Préparation du message à envoyer
		char *msgAEnvoyer = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		strcpy(msgAEnvoyer, pseudoEnvoyeur);
		strcat(msgAEnvoyer, " vous chuchotte : ");
		strcat(msgAEnvoyer, msg);

		// Envoi du message au destinataire
		printf("Envoi du message de %s au clients %s.\n", pseudoEnvoyeur, pseudoRecepteur);
		envoiPrive(pseudoRecepteur, msgAEnvoyer);
		free(msgAEnvoyer);
		return 1;
	}
	else if (strcmp(strToken, "/estConnecte") == 0)
	{
		// Récupération du pseudo
		char *pseudoAVerif = (char *)malloc(sizeof(char) * TAILLE_PSEUDO);
		pseudoAVerif = strtok(NULL, " ");
		pseudoAVerif = strtok(pseudoAVerif, "\n");

		// Préparation du message à envoyer
		char *msgAEnvoyer = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 20));
		strcat(msgAEnvoyer, pseudoAVerif);

		if (verifPseudo(pseudoAVerif))
		{
			// Envoi du message au destinataire
			strcat(msgAEnvoyer, " est en ligne\n");
			envoiPrive(pseudoEnvoyeur, msgAEnvoyer);
		}
		else
		{
			// Envoi du message au destinataire
			strcat(msgAEnvoyer, " n'est pas en ligne\n");
			envoiPrive(pseudoEnvoyeur, msgAEnvoyer);
		}

		free(msgAEnvoyer);
		return 1;
	}
	else if (strcmp(strToken, "/aide\n") == 0)
	{
		// Envoie de l'aide au client, un message par ligne
		FILE *fichierCom = NULL;
		fichierCom = fopen("commande.txt", "r");

		fseek(fichierCom, 0, SEEK_END);
		int longueur = ftell(fichierCom);
		fseek(fichierCom, 0, SEEK_SET);

		if (fichierCom != NULL)
		{
			char *toutFichier = (char *)malloc(longueur);
			fread(toutFichier, sizeof(char), longueur, fichierCom);

			envoiPrive(pseudoEnvoyeur, toutFichier);

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
		char *chaineEnLigne = malloc(sizeof(char) * (TAILLE_PSEUDO + 15) * 20); // Tous les 20 utilisateurs envoie de la chaine concaténée
		int compteur = 0;

		for (int i = 0; i < MAX_CLIENT; i++)
		{
			if (tabClient[i].estOccupe)
			{
				compteur++;
				char *msgAEnvoyer = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 15));
				strcpy(msgAEnvoyer, tabClient[i].pseudo);
				strcat(msgAEnvoyer, " est en ligne\n"); // Taille du message 15

				strcat(chaineEnLigne, msgAEnvoyer);

				free(msgAEnvoyer);
			}
			if (compteur == 20)
			{
				envoiPrive(pseudoEnvoyeur, chaineEnLigne);
				strcpy(chaineEnLigne, "");
				compteur = 0;
			}
		}
		if (compteur != 0)
		{
			envoiPrive(pseudoEnvoyeur, chaineEnLigne);
		}
		free(chaineEnLigne);

		return 1;
	}
	else if (strcmp(strToken, "/déposer\n") == 0)
	{
		long i = pseudoToInt(pseudoEnvoyeur);

		if (i == -1)
		{
			perror("Pseudo pas trouvé");
			exit(-1);
		}

		pthread_t copieFichier;

		if (pthread_create(&copieFichier, NULL, copieFichierThread, (void *)i) == -1)
		{
			perror("Erreur création du thread");
		}

		return 1;
	}
	else if (strcmp(strToken, "/télécharger\n") == 0)
	{
		long i = pseudoToInt(pseudoEnvoyeur);

		if (i == -1)
		{
			perror("Pseudo pas trouvé");
			exit(-1);
		}

		// Acceptons une connexion
		struct sockaddr_in aC;
		socklen_t lg = sizeof(struct sockaddr_in);
		tabClient[i].dSCFC = accept(dS_fichier, (struct sockaddr *)&aC, &lg);

		if (tabClient[i].dSCFC < 0)
		{
			perror("Problème lors de l'acceptation du client\n");
			exit(-1);
		}

		char *afficheFichiers = malloc(sizeof(char) * TAILLE_MESSAGE);

		DIR *dossier;
		struct dirent *entry;
		int fichiers = 0;
		char numFile[10];

		dossier = opendir(DOSSIER_SERVEUR);

		if (dossier == NULL)
		{
			perror("Impossible d'ouvrir le dossier");
			exit(EXIT_FAILURE);
		}

		entry = readdir(dossier);
		entry = readdir(dossier);
		strcpy(afficheFichiers, "Liste des fichiers disponibles :\n");

		while ((entry = readdir(dossier)))
		{
			sprintf(numFile, "%d", fichiers);
			strcat(afficheFichiers, "File ");
			strcat(afficheFichiers, numFile);
			strcat(afficheFichiers, ": ");
			strcat(afficheFichiers, entry->d_name);
			strcat(afficheFichiers, "\n");
			fichiers++;
		}

		closedir(dossier);

		if (send(tabClient[i].dSCFC, afficheFichiers, strlen(afficheFichiers) + 1, 0) == -1)
		{
			perror("Erreur à l'envoi du mp");
			exit(-1);
		}

		int numFichier;

		if (recv(tabClient[i].dSCFC, &numFichier, sizeof(int), 0) == -1)
		{
			perror("Erreur à la réception");
			exit(-1);
		}

		dossier = opendir(DOSSIER_SERVEUR);

		if (dossier == NULL)
		{
			perror("Impossible d'ouvrir le dossier serveur");
			exit(EXIT_FAILURE);
		}

		entry = readdir(dossier);
		entry = readdir(dossier);
		int j = 0;

		while (j <= numFichier)
		{
			entry = readdir(dossier);
			j++;
		}

		char *nomFichier = entry->d_name;

		closedir(dossier);

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
		int numSalon = donnerNumSalon();

		if (numSalon == -1)
		{
			envoiPrive(pseudoEnvoyeur, "Le maximum de salon est atteint vous ne pouvez pas en créer pour le moment\n");
			pthread_mutex_unlock(&mutexSalon);
			return 1;
		}

		strToken = strtok(NULL, "");
		strToken = strtok(strToken, "\n");
		strToken = strtok(strToken, " ");

		if (strToken == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			pthread_mutex_unlock(&mutexSalon);
			return 1;
		}

		char *nomSalon = malloc(sizeof(char) * TAILLE_NOM_SALON);
		strcpy(nomSalon, strToken);

		strToken = strtok(NULL, " ");

		int nbPlaces;

		if (strToken == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			free(nomSalon);
			pthread_mutex_unlock(&mutexSalon);
			return 1;
		}
		else if (atoi(strToken) < 1)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			free(nomSalon);
			pthread_mutex_unlock(&mutexSalon);
			return 1;
		}
		else if (atoi(strToken) > MAX_CLIENT)
		{
			nbPlaces = MAX_CLIENT;
		}
		else
		{
			nbPlaces = atoi(strToken);
		}

		strToken = strtok(NULL, "");

		if (strToken == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			free(nomSalon);
			pthread_mutex_unlock(&mutexSalon);
			return 1;
		}

		char *description = malloc(sizeof(char) * TAILLE_DESCRIPTION);
		strcpy(description, strToken);
		strcat(description, "\n");

		printf("Nom: %s\n", nomSalon);

		printf("Nb: %d\n", nbPlaces);

		printf("desc: %s\n", description);

		tabSalon[numSalon].idSalon = numSalon;
		tabSalon[numSalon].nom = nomSalon;
		tabSalon[numSalon].nbPlace = nbPlaces;
		tabSalon[numSalon].description = description;
		tabSalon[numSalon].estOccupe = 1;
		ecritureSalon();
		pthread_mutex_unlock(&mutexSalon);

		envoiPrive(pseudoEnvoyeur, "Le salon a bien été créé\n");

		return 1;
	}
	else if (strcmp(strToken, "/liste\n") == 0)
	{
		afficheSalon(pseudoEnvoyeur);
		return 1;
	}
	else if (strcmp(strToken, "/suppression") == 0)
	{
		char *nomSalon = strtok(NULL, " ");
		nomSalon = strtok(nomSalon, "\n");

		if (nomSalon == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Vous devez rajouter le nom du salon après /suppression\n\"Faites \"/aide\" pour plus d'informations\n");
			return 1;
		}

		int i = 0;
		while (i < MAX_SALON)
		{
			if (tabSalon[i].estOccupe && strcmp(tabSalon[i].nom, nomSalon) == 0)
			{
				break;
			}
			i++;
		}
		if (i == MAX_SALON)
		{
			envoiPrive(pseudoEnvoyeur, "Le nom du salon n'existe pas\n");
		}
		else
		{
			for (int j = 0; j < MAX_CLIENT; j++)
			{
				if (tabClient[j].estOccupe && tabClient[j].idSalon == tabSalon[i].idSalon)
				{
					tabClient[j].idSalon = 0;
					envoiPrive(tabClient[j].pseudo, "Vous avez été envoyé sur le salon générale\n");
				}
			}

			pthread_mutex_lock(&mutexSalon);
			tabSalon[i].estOccupe = 0;
			ecritureSalon();
			pthread_mutex_unlock(&mutexSalon);

			envoiPrive(pseudoEnvoyeur, "Le salon a été supprimé\n");
		}

		return 1;
	}
	else if (strcmp(strToken, "/kick") == 0 || strcmp(strToken, "/quick") == 0)
	{
		char *pseudoToKick = strtok(NULL, " ");
		pseudoToKick = strtok(pseudoToKick, "\n");

		if (pseudoToKick == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Vous devez rajouter le pseudo de l'utilisateur après /kick\nFaites \"/aide\" pour plus d'informations\n");
			return 1;
		}

		// Vérifier qu ils sont sur le même salon
		int i = pseudoToInt(pseudoToKick);

		if (i == -1)
		{
			envoiPrive(pseudoEnvoyeur, "Le pseudo n'existe pas ou n'est pas connécté\n");
			return 1;
		}

		int j = pseudoToInt(pseudoEnvoyeur);

		if (j == -1)
		{
			perror("Pseudo non trouvé\n");
			exit(-1);
		}

		if (tabClient[i].idSalon != tabClient[j].idSalon)
		{
			envoiPrive(pseudoEnvoyeur, "Vous n'êtes pas sur le même salon que la personne\n");
			return 1;
		}
		else if (tabClient[i].idSalon == 0)
		{
			envoiPrive(pseudoEnvoyeur, "Vous ne pouvez pas kick un utilisateur du chat général\n");
			return 1;
		}

		tabClient[i].idSalon = 0;

		envoiPrive(pseudoToKick, "Vous avez été kick du salon\nVous voilà sur le salon général\n");

		char *rep = malloc(sizeof(char) * (TAILLE_PSEUDO + 24));
		strcpy(rep, tabClient[i].pseudo);
		strcat(rep, " a été kick du salon\n"); // 24
		envoi(dS, rep, tabClient[j].idSalon);

		return 1;
	}
	else if (strcmp(strToken, "/modif") == 0)
	{
		char *nomSalon = strtok(NULL, " ");
		int i = pseudoToInt(pseudoEnvoyeur);

		if (i == -1)
		{
			perror("Le client n'existe pas\n");
			exit(-1);
		}

		// Verification que nomSalon n'est pas NULL
		if (nomSalon == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Il est nécessaire de mettre le nom du salon à modifié\nFaites \"/aide\" pour plus d'informations\n");
			return 1;
		}

		char *modifications = malloc(sizeof(char) * (TAILLE_DESCRIPTION + TAILLE_NOM_SALON + 5));

		reception(tabClient[i].dSC, modifications, sizeof(char) * (TAILLE_DESCRIPTION + TAILLE_NOM_SALON + 10));

		int j = 0;

		while (j < MAX_SALON)
		{
			if (tabSalon[j].estOccupe && strcmp(tabSalon[j].nom, nomSalon) == 0)
			{
				break;
			}
			j++;
		}

		if (j == MAX_SALON)
		{
			envoiPrive(pseudoEnvoyeur, "Le salon n'existe pas\nVous pouvez vérifier la liste des salon avec la commande \"/liste\"\n");
			return 1;
		}

		int numSalon = j;

		modifications = strtok(modifications, "\n");
		modifications = strtok(modifications, " ");

		if (modifications == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			return 1;
		}

		strcpy(nomSalon, modifications);

		modifications = strtok(NULL, " ");

		int nbPlaces;

		if (modifications == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			return 1;
		}
		else if (atoi(modifications) < 1)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			return 1;
		}
		else if (atoi(modifications) > MAX_CLIENT)
		{
			nbPlaces = MAX_CLIENT;
		}
		else
		{
			nbPlaces = atoi(modifications);
		}

		modifications = strtok(NULL, "");

		if (modifications == NULL)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation de la création de salon\nUtilisation de la commande \"/créer\" érronnée\nFaites \"/aide\" pour plus d'informations\n");
			return 1;
		}

		char *description = malloc(sizeof(char) * TAILLE_DESCRIPTION);
		strcpy(description, modifications);

		printf("Nom: %s\n", nomSalon);

		printf("Nb: %d\n", nbPlaces);

		printf("desc: %s\n", description);

		pthread_mutex_lock(&mutexSalon);
		tabSalon[numSalon].nom = nomSalon;
		tabSalon[numSalon].nbPlace = nbPlaces;
		tabSalon[numSalon].description = description;
		tabSalon[numSalon].estOccupe = 1;
		ecritureSalon();
		pthread_mutex_unlock(&mutexSalon);

		envoiPrive(pseudoEnvoyeur, "Le salon a bien été modifé\n");

		return 1;
	}
	else if (strcmp(strToken, "/connexionSalon\n") == 0)
	{
		char *numSalonChar = malloc(sizeof(char) * 5);
		int numSalon; // num salon
		int i = pseudoToInt(pseudoEnvoyeur);

		if (i == -1)
		{
			perror("Pseudo pas trouvé");
			exit(-1);
		}

		afficheSalon(pseudoEnvoyeur);
		sleep(0.2);
		envoiPrive(pseudoEnvoyeur, "Rentrez le numéro de salon souhaité. Si vous souhaitez annuler, tapez -1\n");

		reception(tabClient[i].dSC, numSalonChar, sizeof(char) * 5);

		if (strcmp(numSalonChar, "-1\n") == 0)
		{
			numSalon = -1;
		}
		else
		{
			numSalon = atoi(numSalonChar);
		}

		if (numSalon == -1)
		{
			envoiPrive(pseudoEnvoyeur, "Annulation du changement de salon.\n");
		}
		else if (salonExiste(numSalon) && tabSalon[numSalon].estOccupe && accepteNouvelUtilisateur(numSalon))
		{
			tabClient[i].idSalon = numSalon;
			envoiPrive(pseudoEnvoyeur, "Vous avez été déplacé dans le salon\n");
		}
		else
		{
			envoiPrive(pseudoEnvoyeur, "Ce salon comporte trop de membres ou n'existe pas, veuillez réessayer plus tard.\n");
		}

		free(numSalonChar);

		return 1;
	}
	else if (strToken[0] == '/')
	{
		envoiPrive(pseudoEnvoyeur, "Faites \"/aide\" pour avoir accès aux commandes disponibles et leur fonctionnement\n");
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
	char *pseudo = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 29)); // voir Ligne 1330
	reception(tabClient[numClient].dSC, pseudo, sizeof(char) * TAILLE_PSEUDO);
	pseudo = strtok(pseudo, "\n");

	while (pseudo == NULL || verifPseudo(pseudo))
	{
		send(tabClient[numClient].dSC, "Pseudo déjà existant\n", strlen("Pseudo déjà existant\n") + 1, 0);
		reception(tabClient[numClient].dSC, pseudo, sizeof(char) * TAILLE_PSEUDO);
		pseudo = strtok(pseudo, "\n");
	}

	tabClient[numClient].pseudo = (char *)malloc(sizeof(char) * TAILLE_PSEUDO);
	strcpy(tabClient[numClient].pseudo, pseudo);
	tabClient[numClient].idSalon = 0;

	// On envoie un message pour dire au client qu'il est bien connecté
	char *repServ = (char *)malloc(sizeof(char) * 61);
	repServ = "Entrer /aide pour avoir la liste des commandes disponibles\n"; // 61
	envoiPrive(pseudo, repServ);

	// On vérifie que ce n'est pas le pseudo par défaut
	if (strcmp(pseudo, "FinClient") != 0)
	{
		// On envoie un message pour avertir les autres clients de l'arrivée du nouveau client
		strcat(pseudo, " a rejoint la communication\n"); // 29
		envoi(tabClient[numClient].dSC, pseudo, 0);
	}

	// On a un client en plus sur le serveur, on incrémente
	pthread_mutex_lock(&mutexTabClient);
	nbClient += 1;
	pthread_mutex_unlock(&mutexTabClient);

	printf("Clients connectés : %ld\n", nbClient);

	int estFin = 0;
	char *pseudoEnvoyeur = tabClient[numClient].pseudo;

	while (!estFin)
	{
		// Réception du message
		char *msgReceived = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		reception(tabClient[numClient].dSC, msgReceived, sizeof(char) * TAILLE_MESSAGE);
		printf("\nMessage recu: %s \n", msgReceived);

		// On verifie si le client veut terminer la communication
		estFin = finDeCommunication(msgReceived);

		// On vérifie si le client utilise une des commandes
		char *msgToVerif = (char *)malloc(sizeof(char) * strlen(msgReceived));
		strcpy(msgToVerif, msgReceived);

		if (utilisationCommande(msgToVerif, pseudoEnvoyeur))
		{
			free(msgReceived);
			continue;
		}

		// Ajout du pseudo de l'expéditeur devant le message à envoyer
		char *msgAEnvoyer = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 4 + strlen(msgReceived)));
		strcpy(msgAEnvoyer, pseudoEnvoyeur);
		strcat(msgAEnvoyer, " : ");
		strcat(msgAEnvoyer, msgReceived);
		free(msgReceived);

		// Envoi du message aux autres clients
		printf("Envoi du message aux %ld clients. \n", nbClient - 1);
		envoi(tabClient[numClient].dSC, msgAEnvoyer, tabClient[numClient].idSalon);

		free(msgAEnvoyer);
	}

	// Fermeture du socket client
	pthread_mutex_lock(&mutexTabClient);
	nbClient = nbClient - 1;
	tabClient[numClient].estOccupe = 0;
	free(tabClient[numClient].pseudo);
	pthread_mutex_unlock(&mutexTabClient);

	shutdown(tabClient[numClient].dSC, 2);

	// On relache le sémaphore pour les clients en attente
	sem_post(&semaphoreNbClients);

	// On incrémente le sémaphore des threads
	sem_wait(&semaphoreThread);
	endOfThread(numClient);

	return NULL;
}

/**
 * @brief Fonction gérant l'interruption du programme par CTRL+C
 * Correspond à la gestion des signaux.
 *
 * @param sig_num numéro du signal
 */
void sigintHandler(int sig_num)
{
	printf("\nFin du serveur\n");
	if (dS != 0)
	{
		envoiATous("LE SERVEUR S'EST MOMENTANEMENT ARRETE, DECONNEXION...\n");
		envoiATous("Tout ce message est le code secret pour désactiver les clients");

		int i = 0;
		while (i < MAX_CLIENT)
		{
			if (tabClient[i].estOccupe)
			{
				endOfThread(i);
			}
			i += 1;
		}

		shutdown(dS, 2);
		sem_destroy(&semaphoreNbClients);
		sem_destroy(&semaphoreThread);
		pthread_mutex_destroy(&mutexTabClient);
		pthread_mutex_destroy(&mutexSalon);
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

	// Création du salon général de discussion
	tabSalon[0].idSalon = 0;
	tabSalon[0].estOccupe = 1;
	tabSalon[0].nom = "Chat_général";
	tabSalon[0].description = "Salon général par défaut";
	tabSalon[0].nbPlace = MAX_CLIENT;

	// Vérification et ré-instanciation des différents salons si créés auparavant
	FILE *fic;
	char *ligne = malloc(sizeof(char) * (TAILLE_DESCRIPTION + TAILLE_NOM_SALON + 10));
	int numSalon;
	if ((fic = fopen("fichierSalon.txt", "r")) == NULL)
	{
		fprintf(stderr, "Le fichier 'fichierSalon.txt' n'a pas pu être ouvert\n");
		exit(-1);
	}
	char *strToken;
	while (fgets(ligne, sizeof(char) * (TAILLE_DESCRIPTION + TAILLE_NOM_SALON + 10), fic) != NULL)
	{
		pthread_mutex_lock(&mutexSalon);
		numSalon = atoi(strtok(ligne, " "));
		if (numSalon >= MAX_SALON || numSalon <= 0)
		{
			continue;
		}
		char *nomSalon = malloc(sizeof(char) * TAILLE_NOM_SALON);
		char *desc = malloc(sizeof(char) * TAILLE_DESCRIPTION);
		strToken = strtok(NULL, " ");
		strcpy(nomSalon, strToken);
		tabSalon[numSalon].nom = nomSalon;
		tabSalon[numSalon].nbPlace = atoi(strtok(NULL, " "));
		strToken = strtok(NULL, "");
		strcpy(desc, strToken);
		tabSalon[numSalon].description = desc;
		tabSalon[numSalon].estOccupe = 1;
		pthread_mutex_unlock(&mutexSalon);
	}
	free(ligne);
	if (fclose(fic) < 0)
	{
		fprintf(stderr, "La fermeture du fichier de description des salons a posé problème\n");
		exit(-1);
	}

	// Création de la socket
	dS_fichier = socket(PF_INET, SOCK_STREAM, 0);

	if (dS_fichier < 0)
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

	if (bind(dS_fichier, (struct sockaddr *)&ad_file, sizeof(ad_file)) < 0)
	{
		perror("[Fichier] Erreur lors du nommage de la socket");
		exit(-1);
	}
	printf("[Fichier] Socket nommée\n");

	// Passage de la socket en mode écoute
	if (listen(dS_fichier, 7) < 0)
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
	sem_init(&semaphoreNbClients, PTHREAD_PROCESS_SHARED, MAX_CLIENT);

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
		sem_wait(&semaphoreNbClients);

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
		pthread_mutex_lock(&mutexTabClient);
		long numClient = donnerNumClient();
		tabClient[numClient].estOccupe = 1;
		tabClient[numClient].dSC = dSC;
		tabClient[numClient].pseudo = malloc(sizeof(char) * TAILLE_PSEUDO);
		strcpy(tabClient[numClient].pseudo, " ");
		pthread_mutex_unlock(&mutexTabClient);

		//_____________________ Communication _____________________
		if (pthread_create(&tabThread[numClient], NULL, communication, (void *)numClient) == -1)
		{
			perror("Erreur thread create");
		}
	}
	// ############  	N'arrive jamais  	####################
	shutdown(dS, 2);
	sem_destroy(&semaphoreNbClients);
	sem_destroy(&semaphoreThread);
	pthread_mutex_destroy(&mutexTabClient);
	printf("Fin du programme\n");
	// #########################################################
}