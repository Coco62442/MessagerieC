// #######  	CHANGEMENTS  	##############
// Corentin :
// les threads sont instancies en global
// si receiving recoit le code de deconnexion serveur receiving s arrete et kill thread sending pr termine le client
// ajout de la variable portServeur
// envoieFichier comme pr le serveur retouchée
// changements ds useOfCommands

// Lexay :
// les printfs ont des couleurs : fin de communication = yellow
//                                msg reçus = green
//                                msg système = magenta
//                                msg d'erreur = red
// déclaration des fonctions au début du client
// remplacement des exit(-1) qui étaient trop brutaux

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

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

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

// Déclaration des fonctions
int endOfCommunication(char *msg);
void sending(char *msg);
void *envoieFichier();
void *receptionFichier(void *ds);
int useOfCommand(char *msg);
void *sendingForThread();
void *receivingForThread();
void sigintHandler(int sig_num);

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
        fprintf(stderr, ANSI_COLOR_RED "Votre message n'a pas pu être envoyé\n" ANSI_COLOR_RESET);
        return;
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
        fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Problème de création de socket client\n" ANSI_COLOR_RESET);
        return;
    }

    // Nommage de la socket
    aS.sin_family = AF_INET;
    inet_pton(AF_INET, addrServeur, &(aS.sin_addr));
    aS.sin_port = htons(portServeur + 1);
    socklen_t lgA = sizeof(struct sockaddr_in);

    // Envoi d'une demande de connexion
    printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Connection en cours...\n" ANSI_COLOR_RESET);
    if (connect(dS_file, (struct sockaddr *)&aS, lgA) < 0)
    {
        fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Problème de connexion au serveur\n" ANSI_COLOR_RESET);
        return;
    }
    printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Socket connectée\n" ANSI_COLOR_RESET);

    // DEBUT ENVOI FICHIER
    char *path = malloc(sizeof(char) * 40);
    strcpy(path, "fichiers_client/");
    strcat(path, fileName);

    FILE *stream = fopen(path, "r");
    if (stream == NULL)
    {
        fprintf(stderr, "[ENVOI FICHIER] Le fichier n'a pas pu être ouvert en écriture\n" ANSI_COLOR_RESET);
        return;
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
        fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur au send taille du fichier\n" ANSI_COLOR_RESET);
        return;
    }
    if (send(dS_file, fileName, sizeof(char) * 20, 0) == -1)
    {
        fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur au send nom du fichier\n" ANSI_COLOR_RESET);
        return;
    }
    if (send(dS_file, toutFichier, sizeof(char) * tailleFichier, 0) == -1)
    {
        fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur au send contenu du fichier\n" ANSI_COLOR_RESET);
        return;
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
        fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv de la taille du fichier\n" ANSI_COLOR_RESET);
        return;
    }

    if (recv(ds_file, fileName, sizeof(char) * 100, 0) == -1)
    {
        fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv du nom du fichier\n" ANSI_COLOR_RESET);
        return;
    }

    char *buffer = malloc(sizeof(char) * tailleFichier);

    if (recv(ds_file, buffer, sizeof(char) * tailleFichier, 0) == -1)
    {
        fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv du contenu du fichier\n" ANSI_COLOR_RESET);
        return;
    }
    printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] %s\n" ANSI_COLOR_RESET, buffer);
    printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Taille fichier = %d\n" ANSI_COLOR_RESET, tailleFichier);

    char *emplacementFichier = malloc(sizeof(char) * 120);
    strcpy(emplacementFichier, "./fichiers_client/");
    strcat(emplacementFichier, fileName);
    FILE *stream = fopen(emplacementFichier, "w");
    if (stream == NULL)
    {
        fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Impossible d'ouvrir le fichier en écriture\n" ANSI_COLOR_RESET);
        return;
    }

    if (tailleFichier != fwrite(buffer, sizeof(char), tailleFichier, stream))
    {
        fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Impossible d'écrire dans le fichier\n" ANSI_COLOR_RESET);
        return;
    }

    fseek(stream, 0, SEEK_END);
    int length = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    returnCode = fclose(stream);
    if (returnCode == EOF)
    {
        fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Problème de fermeture du fichier\n" ANSI_COLOR_RESET);
        return;
    }

    if (tailleFichier != length)
    {
        remove(emplacementFichier);
        shutdown(ds_file, 2);
        printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Un problème est survenu\n" ANSI_COLOR_RESET);
        printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Veuillez choisir de nouveau le fichier que vous désirez\n" ANSI_COLOR_RESET);
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
            fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Impossible d'ouvrir le dossier\n" ANSI_COLOR_RESET);
            return 1;
        }
        entry = readdir(folder);
        entry = readdir(folder);
        while ((entry = readdir(folder)))
        {

            tabFichier[files] = entry->d_name;
            printf(ANSI_COLOR_MAGENTA "File %d: %s\n" ANSI_COLOR_RESET,
                   files,
                   entry->d_name);
            files++;
        }

        closedir(folder);

        int rep;
        scanf("%d", &rep);
        printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Fichier voulu %d\n" ANSI_COLOR_RESET, rep);
        while (rep < 0 || rep >= files)
        {
            printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] REP1 :%d\n" ANSI_COLOR_RESET, rep);
            printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] Veuillez entrer un numéro valide\n" ANSI_COLOR_RESET);
            scanf("%d", &rep);
            printf(ANSI_COLOR_MAGENTA "[ENVOI FICHIER] REP2 :%d\n" ANSI_COLOR_RESET, rep);
        }

        printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, tabFichier[rep]);
        strcpy(nomFichier, tabFichier[rep]);

        printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, nomFichier);

        pthread_t thread_envoieFichier;

        if (pthread_create(&thread_envoieFichier, NULL, envoieFichier, 0) < 0)
        {
            fprintf(stderr, ANSI_COLOR_RED "[ENVOI FICHIER] Erreur de création de thread de récéption client\n" ANSI_COLOR_RESET);
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
        if (connect(dS_file, (struct sockaddr *)&aS, lgA) < 0)
        {
            fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Problème de connexion au serveur\n" ANSI_COLOR_RESET);
            return 1;
        }
        printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] Socket connectée\n" ANSI_COLOR_RESET);

        char *listeFichier = malloc(sizeof(char) * 300);
        if (recv(dS_file, listeFichier, sizeof(char) * 300, 0) == -1)
        {
            printf(ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur au recv de la liste des fichiers\n" ANSI_COLOR_RESET);
            return 1;
        }
        printf(ANSI_COLOR_MAGENTA "%s" ANSI_COLOR_RESET, listeFichier);

        char *numFichier = malloc(sizeof(char) * 5);
        fgets(numFichier, 5, stdin);
        printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] numChoisi %s\n" ANSI_COLOR_RESET, numFichier);
        printf(ANSI_COLOR_MAGENTA "[RECEPTION FICHIER] strlen %ld\n" ANSI_COLOR_RESET, strlen(numFichier));
        int intNumFichier = atoi(numFichier);

        if (send(dS_file, &intNumFichier, sizeof(int), 0) == -1)
        {
            fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur à l'envoi de la taille du fichier" ANSI_COLOR_RESET);
            return 1;
        }
        free(numFichier);

        pthread_t thread_receiving_file;
        if (pthread_create(&thread_receiving_file, NULL, receptionFichier, (void *)(long)dS_file) < 0)
        {
            fprintf(stderr, ANSI_COLOR_RED "[RECEPTION FICHIER] Erreur de création de thread de récéption client\n" ANSI_COLOR_RESET);
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
        printf(ANSI_COLOR_YELLOW "** fin de la communication **\n" ANSI_COLOR_RESET);
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

        // strcpy(r, strcat(r, "\n4 > "));
        printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET, r);
        free(r);
    }
    shutdown(dS, 2);
    pthread_cancel(thread_sending);
    return;
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
    char *myPseudo = (char *)malloc(sizeof(char) * 12);
    do
    {
        printf(ANSI_COLOR_MAGENTA "Votre pseudo (maximum 11 caractères):\n" ANSI_COLOR_RESET);
        fgets(myPseudo, 12, stdin);
    } while (strcmp(myPseudo, "\n") == 0);

    // Envoie du pseudo
    sending(myPseudo);

    char *repServeur = (char *)malloc(sizeof(char) * 60);
    // Récéption de la réponse du serveur
    receiving(repServeur, sizeof(char) * 60);
    printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, repServeur);

    while (strcmp(repServeur, "Pseudo déjà existant\n") == 0)
    {
        // Saisie du pseudo du client au clavier
        printf(ANSI_COLOR_MAGENTA "Votre pseudo (maximum 11 caractères):\n" ANSI_COLOR_RESET);
        fgets(myPseudo, 12, stdin);

        // Envoie du pseudo
        sending(myPseudo);

        // Récéption de la réponse du serveur
        receiving(repServeur, sizeof(char) * 60);
        printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, repServeur);
    }
    free(myPseudo);
    printf(ANSI_COLOR_MAGENTA "Connexion complète\n" ANSI_COLOR_RESET);
    boolConnect = 1;

    //_____________________ Communication _____________________

    if (pthread_create(&thread_sending, NULL, sendingForThread, 0) < 0)
    {
        fprintf(stderr, ANSI_COLOR_RED "Erreur de création de thread d'envoi client\n" ANSI_COLOR_RESET);
        exit(-1);
    }

    if (pthread_create(&thread_receiving, NULL, receivingForThread, 0) < 0)
    {
        fprintf(stderr, ANSI_COLOR_RED "Erreur de création de thread réception client\n" ANSI_COLOR_RESET);
        exit(-1);
    }

    pthread_join(thread_sending, NULL);
    pthread_join(thread_receiving, NULL);

    printf(ANSI_COLOR_YELLOW "Fin du programme\n" ANSI_COLOR_RESET);

    return 1;
}