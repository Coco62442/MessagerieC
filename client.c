#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>

#define SIZE 1024
char *DOSSIER_ENVOI_FICHIERS = "./fichiers_client";

int isEnd = 0;
int dS = -1;
int boolConnect = 0;
pthread_t thread_files;
char *addrServeur;
char *portServeur;

/*
 * Envoie un message à une socket et teste que tout se passe bien
 * Paramètres : char * msg : message à envoyer
 */
void sending(char *msg)
{
    if (send(dS, msg, strlen(msg) + 1, 0) == -1)
    {
        perror("Erreur au send");
        exit(-1);
    }
}

void *sendFileForThread(void *fileName)
{
    char *filename = (char *)malloc(100);
    strcpy(filename, (char *)fileName);
    printf("sfft: %s\n", filename);
    // Création de la socket
    int dS_file = socket(PF_INET, SOCK_STREAM, 0);
    if (dS_file == -1)
    {
        perror("[FICHIER] Problème de création de socket client\n");
        exit(-1);
    }
    printf("[FICHIER] Socket Créé\n");

    // Nommage de la socket
    struct sockaddr_in aS_fileTransfer;
    aS_fileTransfer.sin_family = AF_INET;
    inet_pton(AF_INET, addrServeur, &(aS_fileTransfer.sin_addr));
    aS_fileTransfer.sin_port = htons(atoi(portServeur) + 1);
    socklen_t lgA = sizeof(struct sockaddr_in);

    // Envoi d'une demande de connexion
    printf("[FICHIER] Connection en cours...\n");
    if (connect(dS_file, (struct sockaddr *)&aS_fileTransfer, lgA) < 0)
    {
        perror("[FICHIER] Problème de connexion au serveur\n");
        exit(-1);
    }
    printf("[FICHIER] Socket connectée\n");

    // DEBUT ENVOI FICHIER
    int fileSelected = 0;
    char *path = malloc(300);
    FILE *stream = NULL;
    while (!fileSelected)
    {
        printf("je rentre\n");
        strcpy(path, DOSSIER_ENVOI_FICHIERS);
        strcat(path, "/");
        strcat(path, filename);
        printf("%s\n", path);

        stream = fopen(path, "r");
        if (stream == NULL)
        {
            printf("[FICHIER] Ce fichier n'existe pas : %s\n", filename);
        }
        else
        {
            fileSelected = 1;
            continue;
        }

        // Affiche la liste des fichiers
        DIR *folder;
        struct dirent *entry;
        int files = 0;
        folder = opendir(DOSSIER_ENVOI_FICHIERS);
        if (folder != NULL)
        {
            printf("[FICHIER] Voilà la liste de fichiers :\n");
            while ((entry = readdir(folder)))
            {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                    files++;
                    printf("%3d: %s\n", files, entry->d_name);
                }
            }
            closedir(folder);
        }
        else
        {
            perror("Ne peux pas ouvrir le répertoire");
            exit(-1);
        }

        // Demande le fichier a envoyer
        printf("[FICHIER] Indiquer le nom du fichier en tapant '/déposer nomDuFichier': \n");
        fgets(filename, 100, stdin);
    }

    // if (stream == NULL)
    // {
    //     fprintf(stderr, "[FICHIER] Cannot open file for reading\n");
    //     exit(-1);
    // }
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
    printf("Fichier bien envoyé !\n"); // TODO: maybe mettre une vérif ici, que le serveur ait bien récup
    free(path);
    free(chaine);
    free(toutFichier);
    fclose(stream);
    shutdown(dS_file, 2);
}

/*
 * Vérifie si le client souhaite utiliser une des commandes
 * Paramètres : char *msg : message du client à vérifier
 * Retour : 1 (vrai) si le client utilise une commande, 0 (faux) sinon
 */
int useOfCommand(char *msg)
{
    char *strToken = strtok(msg, " ");
    if (strcmp(strToken, "/déposer") == 0)
    {
        char *filename = (char *)malloc(100);
        filename = strtok(NULL, " ");
        printf("uoc: %s\n", filename);
        if (filename != NULL)
        {
            pthread_create(&thread_files, NULL, sendFileForThread, (void *)filename);
        }
        else
        {
            pthread_create(&thread_files, NULL, sendFileForThread, NULL);
        }
        free(strToken);
        return 1;
    }
    free(strToken);
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
        isEnd = strcmp(m, "/fin\n") == 0;

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

    // Création de la socket
    dS = socket(PF_INET, SOCK_STREAM, 0);
    if (dS == -1)
    {
        perror("Problème de création de socket client\n");
        return -1;
    }
    printf("Socket Créé\n");

    // On stocke l'adresse et le port pour le transfer de fichiers
    addrServeur = argv[1];
    portServeur = argv[2];

    // Nommage de la socket
    struct sockaddr_in aS;
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