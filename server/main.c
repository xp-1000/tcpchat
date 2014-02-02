#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAXPENDING 5 // Nombre de requetes de connexion maximum
#define BUFFSIZE 1081 // Taille du tampon des messages
#define MAX_CLIENT 2 // Nombre de clients maximum simultanés

// Prototypes des fonctions
void Die(char *mess);
void *clientThread(void *arg);
void *disconnectThread(void *arg);

// Variables globales
int countclient = 0; // nombre de clients connectés
struct client
{
    int sock;                       // Socket du client
    pthread_t threadclient;         // thread de reception des messages du client
    pthread_t threaddisconnect;     // thread de monitoring du premier thread pour gérer la déconnexion
    struct sockaddr_in chatclient;  // informations réseaux du client
} clients[MAX_CLIENT]; // Tableaux des clients

int main(int argc, char *argv[]) {

    int serversock; // Socket serveur
    struct sockaddr_in chatserver; // Infos réseaux du serveur
    int inc;

    for ( inc=0; inc<MAX_CLIENT; inc++ )
    {
        clients[inc].sock = -1;
    }

    // Vérification du paramètre de port TCP
    if (argc < 1) {
        fprintf(stderr, "Agument expected, please provide a tcp port.\nExample : ./chatserver [port]\n");
        exit(1);
    }

    // Création du socket serveur TCP
    if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        Die("Impossible de créer le socket serveur");
    }

    // Paramètrage du socket serveur dans une structure sockaddr_in
    memset(&chatserver, 0, sizeof(chatserver));       // Effacement de la structure du socket
    chatserver.sin_family = AF_INET;                  // Paramètre de type Internet IP
    chatserver.sin_addr.s_addr = htonl(INADDR_ANY);   // Paramètre de l'ecoute sur toutes les interfaces
    chatserver.sin_port = htons(atoi(argv[1]));       // Paramètre du port TCP

    // Bind du socket serveur
    if (bind(serversock, (struct sockaddr *) &chatserver, sizeof(chatserver)) < 0) {
        Die("Impossible de binder le socket serveur");
    }

    // Mise à l'écoute du socket serveur
    if (listen(serversock, MAXPENDING) < 0) {
        Die("Impossible d'écouter sur le socket serveur");
    }

    // socket temporaire pour les clients refusés en cas de surcharge du serveur
    int tmpsock;
    int waitingclient = 0;

    // Boucle infinie en attente de connexion
    while (1) {

//        fprintf(stdout,"boucle : %d\n", countclient);

        if(countclient < MAX_CLIENT)
        {
            // Si le serveur n'est pas en surchage (slot disponible)

            int i;
            // Parcours du tableau des clients pour trouver quel socket est libre
            for (i=0; i<MAX_CLIENT; i++)
            {
                // Dès qu'un socket apparaît comme disponible (0), sortie de la boucle
                if (clients[i].sock == -1)
                {
                    fprintf(stderr, "TEST : %d\n", clients[i].sock);
                    break;
                }
            }

            // Taille des informations du clients
            unsigned int clientlen = sizeof(clients[i].chatclient);

            if (waitingclient == 1)
            {
                // Si un client était en attente
                // Récupération du socket du client en attente
                clients[i].sock = tmpsock;
                // Plus de client en attente
                waitingclient = 0;
            }
            else
            {
                fprintf(stdout,"Client en connexion \n");
                // Attente de la connexion d'un client
                if ((clients[i].sock = accept(serversock, (struct sockaddr *) &clients[i].chatclient, &clientlen)) < 0) {
                    fprintf(stderr, "ERROR : Impossible d'accepter la socket de connexion");
                }
            }
            // Log de la connexion
            fprintf(stdout, "Client connecté: %s\n", inet_ntoa(clients[i].chatclient.sin_addr));
            // Création du thread de réception des messages pour chaque client
            if (pthread_create(&clients[i].threadclient, NULL, clientThread, &clients[i].sock)) {
                fprintf(stderr,"ERROR : Impossible de créer le thread client");
            }
            // Création d'un thread de moniroting du premier thread qui gèrera la déconnexion du client
            if (pthread_create(&clients[i].threaddisconnect, NULL, disconnectThread, &clients[i].threadclient)) {
                fprintf(stderr, "ERROR : Impossible de créer le thread de monitoring du thread client");
            }
            // Ajout d'un client
            countclient++;
        }
        else
        {
            // Si le serveur est surchargé

            // Effacement de la variable tmpsock
            memset(&tmpsock,0,sizeof(tmpsock));
            // Buffer du message d'erreur a envoyer
            char buffer[BUFFSIZE];
            // Effacement du buffer (au cas où)
            memset(buffer,0,sizeof(buffer));
            // Ajout du message d'erreur au buffer
            strcat(buffer,"ERROR : Nombre de clients maximum atteint");
            // Informations réseaux du client temporaire
            struct sockaddr_in chattmp;
            // Taille des informations réseaux du client temporaire
            unsigned int tmplen = sizeof(chattmp);

            fprintf(stdout,"Nombre de clients max atteint %d\n", tmpsock);

            // Attente de la connexion d'un client
            if ((tmpsock = accept(serversock, (struct sockaddr *) &chattmp, &tmplen)) < 0) {
                fprintf(stderr, "ERROR : Impossible d'accepter la socket de connexion");
            }

            if(countclient < MAX_CLIENT)
            {
                // Si un socket s'est libéré entre la connexion du dernier client max et ce nouveau temporaire
                fprintf(stdout,"client en attente %d\n", tmpsock);
                // Le client est mis en attente pour être traité par l'autre bloc de condition comme un client ordinaire
                waitingclient = 1;
            }
            else
            {
                // Si aucun socket n'est libre
                fprintf(stdout,"Client refusé \n");
                // Envoi du message d'erreur au client refusé
                if (send(tmpsock, buffer, BUFFSIZE, 0) != BUFFSIZE)
                {
                    fprintf(stderr, "Impossible d'envoyer le message d'erreur au client");
                }
                // Fermeture du socket temporaire
                close(tmpsock);
            }
        }
    }
}

// Thread de monitoring de thread client
void *disconnectThread(void *arg)
{
    // Récupération du thread client avec un cast
    pthread_t *thread = (pthread_t *) arg;
    // Attente de la fin du thread client
    pthread_join(*thread, NULL);
    // Suppression d'un client
    countclient--;
    fprintf(stdout, "client déconnecté \n");
    // Fermeture du thread de monitoring
    pthread_exit(NULL);
}

// Thread de reception des massage de client
void *clientThread(void *arg)
{

	int *sock = (int *) arg;
    char buffer[BUFFSIZE];

    while (1) {
        memset(buffer,0,sizeof(buffer));
        // @TODO Ecriture du message reçu dans un fichier texte
        // Réception du message client
        if ((recv(*sock, buffer, BUFFSIZE, 0)) < 1) {
            fprintf(stderr,"ERROR : Impossible de recevoir le message du client\n");
            // Libération du socket client pour un futur client
            *sock = -1;
            // Sortie de la boucle
            break;
        }
        //buffer[BUFFSIZE] = '\0';

        int i;
        for ( i=0; i<MAX_CLIENT; i++ )
        {
            fprintf(stderr, "!!!!!!!!!!!!!!! : %d\n", clients[i].sock);
            // Envoi du message reçu
            if (clients[i].sock != -1 && clients[i].sock != *sock)
            {
                // à tous les clients sauf l'expediteur
                if (send(clients[i].sock, buffer, BUFFSIZE, 0) != BUFFSIZE) {
                    fprintf(stderr, "Impossible d'envoyer le message au client");
                }
            }
        }
        // log des messages
        fprintf(stdout, "Message: %s\n", buffer);
    }

    // Fermeture du socket client
    close(*sock);
    // Fermeture du thread client
    pthread_exit(NULL);
}

// Fonction d'erreur fatale (log et fermeture du programme)
void Die(char *mess)
{
    fprintf(stderr, "ERROR : %s", mess);
    exit(EXIT_FAILURE);
}
