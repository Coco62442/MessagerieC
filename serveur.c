/*
La commande /aide n'est pas totalement implémentée. 
Elle fait crash le serveur lorsque le client souhaite envoyer un message après l'éexécution de la commande
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

/*
 * Définition d'une structure Client pour regrouper toutes les informations du client
 */
typedef struct Client Client;
struct Client
{
	int isOccupied;
	long dSC;
	char *pseudo;
};

/*
 * - MAX_CLIENT = nombre maximum de clients acceptés sur le serveur
 * - tabClient = tableau répertoriant les clients connectés
 * - tabThread = tableau des threads associés au traitement de chaque client
 * - nbClient = nombre de clients actuellement connectés
 */
#define MAX_CLIENT 3
Client tabClient[MAX_CLIENT];
pthread_t tabThread[MAX_CLIENT];
long nbClient = 0;

#define TAILLE_MAX 1000

// Création du sémaphore pour gérer le nombre de client
sem_t semaphore;
// Création du sémpahore pour gérer les threads
sem_t semaphoreThread;
// Création du mutex pour la modification de tabClient[]
pthread_mutex_t mutex;

/*
 * Fonctions pour gérer les indices du tableaux de clients
 * Retour : un entier, indice du premier emplacement disponible
 *          -1 si tout les emplacements sont occupés.
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
	exit(-1);
}

/*
 * Fonctions pour vérifier que le pseudo est unique
 * Retour : un entier
 *          1 si le pseudo existe déjà
 *          0 si le pseudo n'existe pas.
 */
int verifPseudo(char *pseudo)
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (tabClient[i].isOccupied && strcmp(pseudo, tabClient[i].pseudo) == 0) {
			return 1;
		}
		
		i++;
	}
	return 0;
}

/*
 * Envoie un message à toutes les sockets présentes dans le tableau des clients
 * et teste que tout se passe bien
 * Paramètres : int dS : expéditeur du message
 *              char *msg : message à envoyer
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

/*
 * Envoie un message en mp à un client en particulier
 * et teste que tout se passe bien
 * Paramètres : int dSC : destinataire du msg
 *              char *msg : message à envoyer
 */
void sendingDM(char *pseudoReceiver, char *msg)
{
    int i = 0;
    while (i < MAX_CLIENT && tabClient[i].isOccupied && strcmp(tabClient[i].pseudo, pseudoReceiver) != 0)
    {
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

/*
 * Receptionne un message d'une socket et teste que tout se passe bien
 * Paramètres : int dS : la socket
 *              char * msg : message à recevoir
 *              ssize_t size : taille maximum du message à recevoir
 */
void receiving(int dS, char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		perror("Erreur au recv");
		exit(-1);
	}
}

/*
 * Vérifie si un client souhaite quitter la communication
 * Paramètres : char * msg : message du client à vérifier
 * Retour : 1 (vrai) si le client veut quitter, 0 (faux) sinon
 */
int endOfCommunication(char *msg)
{
	if (strcmp(msg, "** a quitté la communication **\n") == 0)
	{
		return 1;
	}
	return 0;
}

/*
 * Permet de JOIN les threads terminés
 * Paramètre : int numClient : indice du thred à join
*/
void endOfThread(int numclient)
{
	pthread_join(tabThread[numclient], 0);
	sem_post(&semaphoreThread);
}

/*
 * Vérifie si un client souhaite utiliser une des commandes
 * Paramètres : char *msg : message du client à vérifier
 * Retour : 1 (vrai) si le client utilise une commande, 0 (faux) sinon
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
			sendingDM(pseudoSender, "Pseudo érronné ou utilisation incorrecte de la commande /mp\n\"/aide\" pour plus d'indication");
            printf("Commande \"/mp\" mal utilisée\n");
            return 0;
        }
        char *msg = (char *)malloc(sizeof(char) * 115);
        msg = strtok(NULL, "");
        if (msg == NULL)
        {
            printf("Commande \"/mp\" mal utilisée\n");
            return 0;
        }
        // Préparation du message à envoyer
        char *msgToSend = (char *)malloc(sizeof(char) * 115);
        strcat(msgToSend, pseudoSender);
        strcat(msgToSend, " vous chuchotte : ");
        strcat(msgToSend, msg);

        // Envoi du message au destinataire
        printf("Envoi du message de %s au clients %s.\n", pseudoSender, pseudoReceiver);
        sendingDM(pseudoReceiver, msgToSend);
		free(msgToSend);
        return 1;
    }
	else if (strcmp(strToken, "/isConnecte") == 0)
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
        FILE* fichierCom = NULL;
        char *chaine = (char *)malloc(sizeof(char) * 100);

        fichierCom = fopen("commande.txt", "r");

        if (fichierCom != NULL)
        {
            while (fgets(chaine, 100, fichierCom) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
            {
				char *chaineToSend = (char *)malloc(sizeof(char) * 100);
				strcpy(chaineToSend, chaine);
				sendingDM(pseudoSender, chaineToSend);
				sleep(0.7);
				free(chaineToSend);
            }
            fclose(fichierCom);
			sendingDM(pseudoSender, "\n");
        }
        else
        {
            // On affiche un message d'erreur si le fichier n'a pas réussi a être ouvert
            printf("Impossible d\'ouvrir le fichier de commande pour l\'aide");
        }
		free(chaine);
        return 1;
    }
	else if(strcmp(strToken, "/enLigne\n") == 0)
	{
		int i = 0;
		while (i < MAX_CLIENT)
		{
			if (tabClient[i].isOccupied)
			{
				char *msgToSend = (char *)malloc(sizeof(char) * 30);
				strcat(msgToSend, tabClient[i].pseudo);
				strcat(msgToSend, " est connecté\n");
				sendingDM(pseudoSender, msgToSend);
				free(msgToSend);
			}
			i++;
		}
		return 1;
	}
    return 0;
}

/*
 * Start routine de pthread_create()
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
            continue;
        }

		// Ajout du pseudo de l'expéditeur devant le message à envoyer
		char *msgToSend = (char *)malloc(sizeof(char) * 115);
		strcat(msgToSend, pseudoSender);
		strcat(msgToSend, " : ");
		strcat(msgToSend, msgReceived);
		free(msgReceived);

		// Envoi du message aux autres clients
		printf("Envoi du message aux %ld clients. \n", nbClient-1);
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

	// Création de la socket
	int dS = socket(PF_INET, SOCK_STREAM, 0);
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
			send(dSC, "Pseudo déjà existant\n",strlen("Pseudo déjà existant\n"), 0);
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
		if (strcmp(pseudo, "FinClient") != 0){
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
		nbClient += 1;
		printf("Clients connectés : %ld\n", nbClient);
	}
	shutdown(dS, 2);
	sem_destroy(&semaphore);
	sem_destroy(&semaphoreThread);
	printf("Fin du programme\n");
}