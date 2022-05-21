#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * Définition d'une structure Client pour regrouper toutes les informations du client
 */
typedef struct Client Client;
struct Client
{
    int isOccupied;
    long dSC;
    char *pseudo;
    long dSCFC;
    long dSCFE;
};

typedef struct Fichier Fichier;
struct Fichier
{
    int emplacementNonDisponible;
    char *nomFichier;
    int tailleFichier;
};

/*
 * - MAX_CLIENT = nombre maximum de clients acceptés sur le serveur
 * - tabClient = tableau répertoriant les clients connectés
 * - tabThread = tableau des threads associés au traitement de chaque client
 * - nbClient = nombre de clients actuellement connectés
 */

#define MAX_CLIENT 3
#define MAX_FICHIER 4
Client tabClient[MAX_CLIENT];
pthread_t tabThread[MAX_CLIENT];
Fichier tabFichier[MAX_FICHIER];
long nbClient = 0;
int dS;
int dS_file;
int nbFichier = 0;

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

int giveNumFichier()
{
    int i = 0;
    while (i < MAX_FICHIER)
    {
        if (!tabFichier[i].emplacementNonDisponible)
        {
            return i;
        }
        i += 1;
    }
    return -1;
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
        if (tabClient[i].isOccupied && strcmp(pseudo, tabClient[i].pseudo) == 0)
        {
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
 * Permet de récupérer le dSC pour un pseudo donné
 * Renvoie -1 si le pseudo n'existe pas
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

/*
 * Envoie un message en mp à un client en particulier
 * et teste que tout se passe bien
 * Paramètres : int dSC : destinataire du msg
 *              char *msg : message à envoyer
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
    if (strcmp(msg, "/fin\n") == 0)
    {
        return 1;
    }
    return 0;
}

void *copieFichierThread(void *clientIndex)
{
    int i = (long)clientIndex;

    printf("%d\n", i);
    // Acceptons une connexion
    struct sockaddr_in aC;
    socklen_t lg = sizeof(struct sockaddr_in);
    tabClient[i].dSCFC = accept(dS_file, (struct sockaddr *)&aC, &lg);
    if (tabClient[i].dSCFC < 0)
    {
        perror("Problème lors de l'acceptation du client\n");
        exit(-1);
    }

    // Réception des informations du fichier
    int tailleFichier;
    char *nomFichier = (char *)malloc(sizeof(char) * 100);
    if (recv(tabClient[i].dSCFC, &tailleFichier, sizeof(int), 0) == -1)
    {
        perror("Erreur au recv");
        exit(-1);
    }
    receiving(tabClient[i].dSCFC, nomFichier, sizeof(char) * 100);
    printf("%s\n", nomFichier);
    printf("%d\n", tailleFichier);

    // Début réception du fichier
    char *buffer = (char *)malloc(sizeof(char) * tailleFichier);
    int returnCode;
    int index;

    char *emplacementFichier = (char *)malloc(sizeof(char) * 50);
    strcat(emplacementFichier, "FichierServeur/");
    strcat(emplacementFichier, nomFichier);
    FILE *stream = fopen(emplacementFichier, "w");
    if (stream == NULL)
    {
        fprintf(stderr, "Cannot open file for writing\n");
        exit(-1);
    }

    receiving(tabClient[i].dSCFC, buffer, tailleFichier);
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
    printf("1\n");
    free(nomFichier);
    printf("2\n");
    free(emplacementFichier);
    printf("3\n");
    int j = giveNumFichier();
    tabFichier[j].emplacementNonDisponible = 1;
    tabFichier[j].nomFichier = nomFichier;
    tabFichier[j].tailleFichier = tailleFichier;
    nbFichier++;

    sendingDM(tabClient[i].pseudo, "Téléchargement du fichier terminé\n");
    shutdown(tabClient[i].dSCFC, 2);
}

void *envoieFichierThread(void *clientIndex)
{
    int i = (long)clientIndex;
    printf("%d\n", i);

    // Acceptons une connexion
    struct sockaddr_in aC;
    socklen_t lg = sizeof(struct sockaddr_in);
    tabClient[i].dSCFC = accept(dS, (struct sockaddr *)&aC, &lg);
    if (tabClient[i].dSCFC < 0)
    {
        perror("Problème lors de l'acceptation du client\n");
        exit(-1);
    }

    char *rep = (char *)malloc(sizeof(char) * 100);
    printf("%d\n", nbFichier);
    for (int j = 0; j < nbFichier; j++)
    {
        strcat(rep, j + "0");
        strcat(rep, "\t");
        printf("%s\n", tabFichier[j].nomFichier);
        strcat(rep, tabFichier[j].nomFichier);
        strcat(rep, "\t");
        strcat(rep, tabFichier[j].tailleFichier + "0");
        strcat(rep, "\n");
    }
    printf("%s\n", rep);
    sendingDM(tabClient[i].pseudo, rep);
    free(rep);

    int numFichier;
    if (recv(tabClient[i].dSCFC, &numFichier, sizeof(int), 0) == -1)
    {
        perror("Erreur au recv");
        exit(-1);
    }

    // DEBUT ENVOI FICHIER
    char *path = malloc(100);
    strcat(path, "./FichierServeur/");
    strcat(path, tabFichier[i].nomFichier);
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
    if (send(tabClient[i].dSCFC, &length, sizeof(int), 0) == -1)
    {
        perror("Erreur au send");
        exit(-1);
    }
    if (send(tabClient[i].dSCFC, tabFichier[i].nomFichier, strlen(tabFichier[i].nomFichier) + 1, 0) == -1)
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
    if (send(tabClient[i].dSCFC, toutFichier, length + 1, 0) == -1)
    {
        perror("Erreur au send");
        exit(-1);
    }
    free(chaine);
    free(toutFichier);
    fclose(stream);
    shutdown(tabClient[i].dSCFC, 2);
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
        FILE *fichierCom = NULL;
        fichierCom = fopen("commande.txt", "r");
        fseek(fichierCom, 0, SEEK_END);
        int length = ftell(fichierCom);
        fseek(fichierCom, 0, SEEK_SET);

        if (fichierCom != NULL)
        {
            char *chaine = (char *)malloc(100);
            char *toutFichier = (char *)malloc(length);
            while (fgets(chaine, 100, fichierCom) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
            {
                strcat(toutFichier, chaine);
            }
            sendingDM(pseudoSender, toutFichier);
            free(chaine);
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
                strcat(msgToSend, tabClient[i].pseudo);
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
        pthread_t envoieFichier;
        int i = 0;
        while (strcmp(tabClient[i].pseudo, pseudoSender) != 0)
        {
            i++;
        }

        if (pthread_create(&envoieFichier, NULL, envoieFichierThread, (void *)(long)i) == -1)
        {
            perror("Erreur thread create");
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
            free(msgReceived);
            continue;
        }

        // Ajout du pseudo de l'expéditeur devant le message à envoyer
        char *msgToSend = (char *)malloc(sizeof(char) * 115);
        strcat(msgToSend, pseudoSender);
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
        nbClient += 1;
        printf("Clients connectés : %ld\n", nbClient);
    }
    shutdown(dS, 2);
    sem_destroy(&semaphore);
    sem_destroy(&semaphoreThread);
    printf("Fin du programme\n");
}