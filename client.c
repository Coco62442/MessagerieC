#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

int isEnd = 0;
int dS = -1;
int boolConnect = 0;

/*
 * Vérifie si un client souhaite quitter la communication
 * Paramètres : char ** msg : message du client à vérifier
 * Retour : 1 (vrai) si le client veut quitter, 0 (faux) sinon
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

/*
 * Fonction pour le thread d'envoi
 */
void *sendingForThread()
{
    while (!isEnd)
    {
        /*Saisie du message au clavier*/
        char *m = (char *)malloc(sizeof(char) * 100);
        printf(">");
        fgets(m, 100, stdin);

        // On vérifie si le client veut quitter la communication
        isEnd = endOfCommunication(m);

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

        printf(">%s", r);
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
    sending("** a quitté la communication **\n");
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

    // Appels bloquants
    pthread_join(thread_sendind, NULL);
    pthread_join(thread_receiving, NULL);
    printf("Fin du programme\n");
}