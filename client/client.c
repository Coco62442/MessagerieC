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
 * Définition des différents codes pour l'utilisation de couleurs dans le texte
 */
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

/**
 * - MAX_SALON = nombre maximum de salons sur le serveur
 * - TAILLE_PSEUDO = taille maximum du pseudo
 * - TAILLE_DESCRIPTION = taille maximum de la description du salon
 * - TAILLE_NOM_SALON = taille maximum du nom du salon
 * - DOSSIER_CLIENT = nom du dossier où sont stockés les fichiers
 * - TAILLE_NOM_FICHIER = taille maximum du nom du fichier
 * - TAILLE_MESSAGE = taille maximum d'un message
 */
#define MAX_SALON 7
#define TAILLE_PSEUDO 20
#define TAILLE_DESCRIPTION 200
#define TAILLE_NOM_SALON 20
#define DOSSIER_CLIENT "fichiers_client"
#define TAILLE_NOM_FICHIER 100
#define TAILLE_MESSAGE 500

/**
 * - nomFichier = nom du fichier à transférer
 * - estFin = booléen vérifiant si le client est connecté ou s'il a terminé la discussion avec le serveur
 * - dS = socket du serveur
 * - boolConnect = booléen vérifiant si le client est connecté afin de gérer les signaux (CTRL+C)
 * - addrServeur = adresse du serveur sur laquelle est connecté le client
 * - portServeur = port du serveur sur lequel est connecté le client
 * - aS = structure contenant toutes les informations de connexion du client au serveur
 * - thread_envoi = thread gérant l'envoi de messages
 * - thread_reception = thread gérant la réception de messages
 */
char nomFichier[20];
int estFin = 0;
int dS = -1;
int boolConnect = 0;
char *addrServeur;
int portServeur;
struct sockaddr_in aS;

// Création des threads
pthread_t thread_envoi;
pthread_t thread_reception;

// Déclaration des fonctions
int finDeCommunication(char *msg);
void envoi(char *msg);
void *envoieFichier();
void *receptionFichier(void *ds);
int utilisationCommande(char *msg);
void *envoiPourThread();
void reception(char *rep, ssize_t size);
void *receptionPourThread();
void sigintHandler(int sig_num);

/**
 * @brief Vérifie si un client souhaite quitter la communication.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client veut quitter, 0 sinon.
 */
int finDeCommunication(char *msg)
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
void envoi(char *msg)
{
	if (send(dS, msg, strlen(msg) + 1, 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "Votre message n'a pas pu être envoyé\n" ANSI_COLOR_RESET);
		return;
	}
}

/**
 * @brief Fonction principale du thread gérant le transfert de fichiers vers le serveur.
 */
void *envoieFichier()
{
	char *nvNomFichier = nomFichier;

	// Création de la socket
	int dS_fichier = socket(PF_INET, SOCK_STREAM, 0);

	if (dS_fichier == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Problème de création de socket client\n" ANSI_COLOR_RESET);
		return NULL;
	}

	// Nommage de la socket
	aS.sin_family = AF_INET;
	inet_pton(AF_INET, addrServeur, &(aS.sin_addr));
	aS.sin_port = htons(portServeur + 1);
	socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
	printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Connection en cours...\n" ANSI_COLOR_RESET);
	if (connect(dS_fichier, (struct sockaddr *)&aS, lgA) < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Problème de connexion au serveur\n" ANSI_COLOR_RESET);
		return NULL;
	}
	printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Socket connectée\n" ANSI_COLOR_RESET);

	printf("%s\n", nvNomFichier);
	// DEBUT ENVOI FICHIER
	char *chemin = malloc(sizeof(char) * (strlen(DOSSIER_CLIENT) + 2 + strlen(nvNomFichier)));
	strcpy(chemin, DOSSIER_CLIENT);
	strcat(chemin, "/");
	strcat(chemin, nvNomFichier);
	
	printf("%s\n", chemin);

	FILE *stream = fopen(chemin, "r");
	if (stream == NULL)
	{
		fprintf(stderr, "[ENVOI FICHIER] Le fichier n'a pas pu être ouvert en écriture\n" ANSI_COLOR_RESET);
		return NULL;
	}

	fseek(stream, 0, SEEK_END);
	int taille = ftell(stream);
	fseek(stream, 0, SEEK_SET);

	// Lecture et stockage pour envoi du fichier
	char *toutFichier = malloc(sizeof(char) * taille);
	int tailleFichier = fread(toutFichier, sizeof(char), taille, stream);
	free(chemin);
	fclose(stream);

	if (send(dS_fichier, &taille, sizeof(int), 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur au send taille du fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}
	if (send(dS_fichier, nvNomFichier, sizeof(char) * 20, 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur au send nom du fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}
	if (send(dS_fichier, toutFichier, sizeof(char) * tailleFichier, 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur au send contenu du fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}
}

/**
 * @brief Fonction principale du thread gérant le transfert de fichier depuis le serveur.
 *
 * @param ds socket du serveur
 */
void *receptionFichier(void *ds)
{

	int dS_fichier = (long)ds;
	int codeRetour;
	int index;

	char *nomFichier = malloc(sizeof(char) * TAILLE_NOM_FICHIER);
	int tailleFichier;

	if (recv(dS_fichier, &tailleFichier, sizeof(int), 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv de la taille du fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}

	if (recv(dS_fichier, nomFichier, sizeof(char) * TAILLE_NOM_FICHIER, 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv du nom du fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}

	char *buffer = malloc(sizeof(char) * tailleFichier);

	if (recv(dS_fichier, buffer, sizeof(char) * tailleFichier, 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv du contenu du fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}

	char *emplacementFichier = malloc(sizeof(char) * (strlen(DOSSIER_CLIENT) + 2 + strlen(nomFichier)));
	strcpy(emplacementFichier, DOSSIER_CLIENT);
	strcat(emplacementFichier, "/");
	strcat(emplacementFichier, nomFichier);

	FILE *stream = fopen(emplacementFichier, "w");
	if (stream == NULL)
	{
		fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Impossible d'ouvrir le fichier en écriture\n" ANSI_COLOR_RESET);
		return NULL;
	}

	if (tailleFichier != fwrite(buffer, sizeof(char), tailleFichier, stream))
	{
		fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Impossible d'écrire dans le fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}

	fseek(stream, 0, SEEK_END);
	int taille = ftell(stream);
	fseek(stream, 0, SEEK_SET);

	codeRetour = fclose(stream);
	if (codeRetour == EOF)
	{
		fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Problème de fermeture du fichier\n" ANSI_COLOR_RESET);
		return NULL;
	}

	if (tailleFichier != taille)
	{
		remove(emplacementFichier);
		shutdown(dS_fichier, 2);
		printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Un problème est survenu\n" ANSI_COLOR_RESET);
		printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Veuillez choisir de nouveau le fichier que vous désirez\n" ANSI_COLOR_RESET);
		utilisationCommande("/télécharger\n");
	}

	free(nomFichier);
	free(buffer);
	free(emplacementFichier);
	shutdown(dS_fichier, 2);
	printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Le fichier a bien été téléchargé\n" ANSI_COLOR_RESET);
}

/**
 * @brief Vérifie si un client souhaite utiliser une des commandes disponibles.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client utilise une commande, 0 sinon.
 */
int utilisationCommande(char *msg)
{
	char *strToken = strtok(msg, " ");
	if (strcmp(strToken, "/déposer\n") == 0)
	{
		envoi(strToken);

		char *tabFichier[50];
		DIR *dossier;
		struct dirent *entry;
		int fichiers = 0;

		dossier = opendir(DOSSIER_CLIENT);
		if (dossier == NULL)
		{
			fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Impossible d'ouvrir le dossier\n" ANSI_COLOR_RESET);
			return 1;
		}

		entry = readdir(dossier);
		entry = readdir(dossier);
		while ((entry = readdir(dossier)))
		{
			tabFichier[fichiers] = entry->d_name;
			printf(ANSI_COLOR_MAGENTA "Fichier %d: %s\n" ANSI_COLOR_RESET,
				   fichiers,
				   entry->d_name);
			fichiers++;
		}

		closedir(dossier);

		int rep;
		scanf("%d", &rep);
		printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Fichier voulu %d\n" ANSI_COLOR_RESET, rep);
		while (rep < 0 || rep >= fichiers)
		{
			printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Veuillez entrer un numéro valide\n" ANSI_COLOR_RESET);
			scanf("%d", &rep);
		}

		strcpy(nomFichier, tabFichier[rep]);

		pthread_t thread_envoieFichier;

		if (pthread_create(&thread_envoieFichier, NULL, envoieFichier, 0) < 0)
		{
			fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur de création de thread de récéption client\n" ANSI_COLOR_RESET);
		}

		return 1;
	}
	else if (strcmp(strToken, "/télécharger\n") == 0)
	{
		envoi(strToken);

		// Création de la socket
		int dS_fichier = socket(PF_INET, SOCK_STREAM, 0);
		if (dS_fichier == -1)
		{
			fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Problème de création de socket client\n" ANSI_COLOR_RESET);
			return 1;
		}
		printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Socket Créé\n" ANSI_COLOR_RESET);

		// Nommage de la socket
		aS.sin_family = AF_INET;
		inet_pton(AF_INET, addrServeur, &(aS.sin_addr));
		aS.sin_port = htons(portServeur + 1);
		socklen_t lgA = sizeof(struct sockaddr_in);

		// Envoi d'une demande de connexion
		printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Connection en cours...\n" ANSI_COLOR_RESET);
		if (connect(dS_fichier, (struct sockaddr *)&aS, lgA) < 0)
		{
			fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Problème de connexion au serveur\n" ANSI_COLOR_RESET);
			return 1;
		}
		printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Socket connectée\n" ANSI_COLOR_RESET);

		char *listeFichier = malloc(sizeof(char) * (TAILLE_NOM_SALON * MAX_SALON));
		if (recv(dS_fichier, listeFichier, sizeof(char) * (TAILLE_NOM_SALON * MAX_SALON), 0) == -1)
		{
			printf(ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv de la liste des fichiers\n" ANSI_COLOR_RESET);
			return 1;
		}
		printf(ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET, listeFichier);

		char *numFichier = malloc(sizeof(char) * 5);
		fgets(numFichier, 5, stdin);

		int intNumFichier = atoi(numFichier);

		if (send(dS_fichier, &intNumFichier, sizeof(int), 0) == -1)
		{
			fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur à l'envoi de la taille du fichier" ANSI_COLOR_RESET);
			return 1;
		}
		free(numFichier);

		pthread_t thread_reception_fichier;

		if (pthread_create(&thread_reception_fichier, NULL, receptionFichier, (void *)(long)dS_fichier) < 0)
		{
			fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur de création de thread de récéption client\n" ANSI_COLOR_RESET);
		}

		return 1;
	}
	else if (strcmp(strToken, "/modif") == 0)
	{
		strToken = strtok(NULL, " ");
		strToken = strtok(strToken, "\n");

		if (strToken == NULL)
		{
			printf("Vous devez rentrer le nom du salon que vous voulez modifier\nFaites \"/aide\" pour plus d'informations\n");
			return 1;
		}

		char *commande = malloc(sizeof(char) * (TAILLE_NOM_SALON + 8));
		strcpy(commande, "/modif ");
		strcat(commande, strToken);

		// Vérification qu'il n'essaye pas de changer le chat général
		if (strcmp(strToken, "Chat_général") == 0)
		{
			printf(ANSI_COLOR_MAGENTA "Vous ne pouvez pas apporter des modifications sur ce salon ;)\n" ANSI_COLOR_RESET);
		}
		else
		{
			envoi(commande);

			free(commande);

			printf(ANSI_COLOR_MAGENTA "Entrez les modifications du salon de la forme:\nNomSalon NbrPlaces Description du salon\n" ANSI_COLOR_RESET);
			char *modifs = malloc(sizeof(char) * (TAILLE_NOM_SALON + TAILLE_DESCRIPTION + 10));

			fgets(modifs, TAILLE_NOM_SALON + TAILLE_DESCRIPTION + 10, stdin);

			envoi(modifs);

			free(modifs);
		}
		return 1;
	}

	return 0;
}

/**
 * @brief Fonction principale pour le thread gérant l'envoi de messages.
 */
void *envoiPourThread()
{
	while (!estFin)
	{
		/*Saisie du message au clavier*/
		char *m = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		fgets(m, TAILLE_MESSAGE, stdin);

		// On vérifie si le client veut quitter la communication
		estFin = finDeCommunication(m);

		// On vérifie si le client utilise une des commandes
		char *msgAVerif = (char *)malloc(sizeof(char) * strlen(m));
		strcpy(msgAVerif, m);

		if (utilisationCommande(msgAVerif))
		{
			free(m);
			continue;
		}

		// Envoi
		envoi(m);
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
void reception(char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		printf(ANSI_COLOR_YELLOW "** fin de la communication **\n" ANSI_COLOR_RESET);
		exit(-1);
	}
}

/**
 * @brief Fonction principale pour le thread gérant la réception de messages.
 */
void *receptionPourThread()
{
	while (!estFin)
	{
		char *r = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		reception(r, sizeof(char) * TAILLE_MESSAGE);
		if (strcmp(r, "Tout ce message est le code secret pour désactiver les clients") == 0)
		{
			free(r);
			break;
		}

		printf("%s", r);
		free(r);
	}
	shutdown(dS, 2);
	pthread_cancel(thread_envoi);
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
	printf(ANSI_COLOR_YELLOW "\nFin du programme avec Ctrl + C \n" ANSI_COLOR_RESET);
	if (!boolConnect)
	{
		char *myPseudoEnd = (char *)malloc(sizeof(char) * 12);
		myPseudoEnd = "FinClient";
		envoi(myPseudoEnd);
	}
	sleep(0.2);
	envoi("/fin\n");
	exit(1);
}

// argv[1] = adresse ip
// argv[2] = port
int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, ANSI_COLOR_RED "Erreur : Lancez avec ./client [votre_ip] [votre_port] " ANSI_COLOR_RESET);
		return -1;
	}
	printf(ANSI_COLOR_MAGENTA "Début programme\n" ANSI_COLOR_RESET);

	addrServeur = argv[1];
	portServeur = atoi(argv[2]);

	// Création de la socket
	dS = socket(PF_INET, SOCK_STREAM, 0);
	if (dS == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "Problème de création de socket client\n" ANSI_COLOR_RESET);
		return -1;
	}
	printf(ANSI_COLOR_MAGENTA "Socket Créé\n" ANSI_COLOR_RESET);

	// Nommage de la socket
	aS.sin_family = AF_INET;
	inet_pton(AF_INET, argv[1], &(aS.sin_addr));
	aS.sin_port = htons(atoi(argv[2]));
	socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
	printf(ANSI_COLOR_MAGENTA "Connexion en cours...\n" ANSI_COLOR_RESET);
	if (connect(dS, (struct sockaddr *)&aS, lgA) < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Problème de connexion au serveur\n" ANSI_COLOR_RESET);
		exit(-1);
	}
	printf(ANSI_COLOR_MAGENTA "Socket connectée\n" ANSI_COLOR_RESET);

	// Fin avec Ctrl + C
	signal(SIGINT, sigintHandler);

	// Saisie du pseudo du client au clavier
	char *monPseudo = (char *)malloc(sizeof(char) * TAILLE_PSEUDO);
	do
	{
		printf(ANSI_COLOR_MAGENTA "Votre pseudo (maximum 19 caractères):\n" ANSI_COLOR_RESET);
		fgets(monPseudo, TAILLE_PSEUDO, stdin);
		for (int i = 0; i < strlen(monPseudo); i++)
		{
			if (monPseudo[i] == ' ')
			{
				monPseudo[i] = '_';
			}
		}
	} while (strcmp(monPseudo, "\n") == 0);

	// Envoie du pseudo
	envoi(monPseudo);

	char *repServeur = (char *)malloc(sizeof(char) * 61);
	// Récéption de la réponse du serveur
	reception(repServeur, sizeof(char) * 61);
	printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, repServeur);

	while (strcmp(repServeur, "Pseudo déjà existant\n") == 0)
	{
		// Saisie du pseudo du client au clavier
		printf(ANSI_COLOR_MAGENTA "Votre pseudo (maximum 19 caractères):\n" ANSI_COLOR_RESET);
		fgets(monPseudo, TAILLE_PSEUDO, stdin);

		for (int i = 0; i < strlen(monPseudo); i++)
		{
			if (monPseudo[i] == ' ')
			{
				monPseudo[i] = '_';
			}
		}

		// Envoie du pseudo
		envoi(monPseudo);

		// Récéption de la réponse du serveur
		reception(repServeur, sizeof(char) * 61);
		printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, repServeur);
	}

	free(monPseudo);
	printf(ANSI_COLOR_MAGENTA "Connexion complète\n" ANSI_COLOR_RESET);
	boolConnect = 1;

	//_____________________ Communication _____________________

	if (pthread_create(&thread_envoi, NULL, envoiPourThread, 0) < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Erreur de création de thread d'envoi client\n" ANSI_COLOR_RESET);
		exit(-1);
	}

	if (pthread_create(&thread_reception, NULL, receptionPourThread, 0) < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Erreur de création de thread réception client\n" ANSI_COLOR_RESET);
		exit(-1);
	}

	pthread_join(thread_envoi, NULL);
	pthread_join(thread_reception, NULL);

	printf(ANSI_COLOR_YELLOW "Fin du programme\n" ANSI_COLOR_RESET);

	return 1;
}