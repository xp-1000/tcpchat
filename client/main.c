#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFSIZE 1024 // Taille du buffer de contenu des messages
#define PSEUDOSIZE 50 // Taille du pseudo de l'utilisateur

// Prototypes des fonctions
void Die(char *mess);
void *waitMessage (void *arg);
void purge(void);
void clean (char *chaine);

int main(int argc, char *argv[]) {

    // Déclaration des variables du client
    int sock; // socket de connexion au serveur
    struct sockaddr_in chatserver; // Infos réseaux du serveur
    char pseudo[PSEUDOSIZE]; // Pseudonyme de l'utilisateur
    pthread_t listen; // Thread de réception des messages

    // Vérification des paramètres d'adresse IP, message et port TCP
    if (argc < 2)
    {
        fprintf(stderr, "\nERROR: Paramètres attendus non trouvés, merci de préciser les bons paramètres \nExemple : clientchat [IP] [PORT]\n");
        //fprintf(stderr, "ERROR: \n\tAguments expected, please provide needed parameters. \n\nNAME: \n\tchat \n\nDESCRIPTION: \n\tCreate a new chat or join an existing chat. \n\nSYNOPSIS: \n\t chat [OPTION] [ARGS_LIST] \n\nOPTIONS: \n\t-s \tCreate a new chat server \n\t-r \tConnect to chat server without authentification. Only read access. \n\t-w \tConnect to chat server with authentification. Read and write access. Need a login. \n\nUSAGE: \n\t./chat -s [server_port] \n\t./chat -r [server_ip] [server_port] \n\t./chat -w [server_ip] [server_port] [login]\n");
        exit(1);
    }

    // Création du socket TCP
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        Die("Impossible de créer le socket de connexion au serveur");
    }

    // Paramètrage du socket dans une structure sockaddr_in
    memset(&chatserver, 0, sizeof(chatserver));             // Effacement de la structure du socket serveur
    chatserver.sin_family = AF_INET;                        // Paramètre de type Internet IP
    chatserver.sin_addr.s_addr = inet_addr(argv[1]);        // Paramètre de l'ecoute sur toutes les interfaces
    chatserver.sin_port = htons(atoi(argv[2]));             // Paramètre du port TCP

    // Tentative du connexion au serveur
    if (connect(sock, (struct sockaddr *) &chatserver, sizeof(chatserver)) < 0)
    {
        Die("Impossible de se connecter au serveur");
    }

    // Capture du pseudonyme de l'utilisateur
    fprintf(stdout, "Choisissez un pseudonyme : \n");
    fgets(pseudo, sizeof pseudo, stdin);

    if (pseudo[0] == 10)
    {
        // Si l'utilisateur ne choisit pas de pseudonyme (touche entrer direct)
        fprintf(stdout, "Authentification en mode spectateur, permission en lecture uniquement\n\n");
        // Fonction de réception des messages
        waitMessage((void*) &sock);
    }
    else
    {
        // Si l'utilisateur a choisit un pseudonyme (écriture possible)
        // Création d'un thread de réception des messages
        if (pthread_create(&listen, NULL, waitMessage, &sock))
        {
            Die("pthread_create");
        }

        // Suppression des "\n" dans le pseudo (au cas où)
        clean(pseudo);
        fprintf(stdout, "Authentification en tant que : %s\n\n",pseudo);

        // Contenu du message à envoyer au serveur
        char in[BUFFSIZE];
        // Message en entier contenant le pseudo et "dit : " et le contenu du message
        char message[BUFFSIZE+PSEUDOSIZE+sizeof(" dit: ")];
        unsigned int messlen; // Taille du message
        // Boucle infinie pour la capture du message de utilisateur
        while (1)
        {
            // Effacement de la variable in
            memset(in,0,sizeof(in));
            // Capture du message de l'utilisateur
            fgets(in, sizeof in, stdin);
            if (in[0] != 10)
            {
                // Si l'utilisateur a saisie quelque chose
                // Effacement de la variable message
                memset(message,0,sizeof(message));
                // Ajout du pseudo au message complet
                strcat(message,pseudo);
                // Ajout du texte " dit : " au message complet
                strcat(message," dit: ");
                // Effacement des "\n" dans de la saisie du contenu du message
                clean(in);
                // Ajout du contenu du message au message complet
                strcat(message,in);
                // Calcul de la taille du message complet
                messlen = strlen(message);
                // Envoi du message au serveur
                if (send(sock, message, messlen, 0) != messlen)
                {
                    Die("Impossible d'envoyer le message au serveur");
                }
            }
            else
            {
                // Si l'utilisateur n'a rien saisi
                fprintf(stdout, "C'est absurde d'envoyer un message vide ...\n");
            }
        }
    }
    // Fermeture du socket de connexion
    close(sock);
    // Fermeture du programme
    exit(0);
}

// Thread de recpetion des messages
void *waitMessage (void *arg)
{
    // Affectation du socket avec un cast sur le paramètre du thread
	int *sock = (int *) arg;
	// Buffer du message recu
	char buffer[BUFFSIZE+PSEUDOSIZE+sizeof(" dit: ")];
	// Boucle de lecture du message du serveur
    while (1) {
        // Effacement du buffer pour chaque nouveau message
        memset(buffer,0,sizeof(buffer));
        // flush de la sortie principale stdout (au cas où)
        fflush(stdout);
        // Réception du message du serveur
        if ((recv(*sock, buffer, BUFFSIZE, 0)) < 1)
        {
            Die("Impossible de recevoir le message du serveur");
        }

        //buffer[BUFFSIZE] = '\0';
        // Impression du message reçu à l'utilisateur
        fprintf(stdout, "%s\n", buffer);
    }
}

// Fonctions de sécurité pour fgets avec suppression du "\n"
void clean (char *chaine)
{
    char *p = strchr(chaine, '\n');

    if (p)
    {
        *p = 0;
    }
    else
    {
        purge();
    }
}

void purge(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
    {}
}

// Fonction d'erreur fatale (log et fermeture du programme)
void Die(char *mess)
{
    fprintf(stderr, "ERROR : %s\n", mess);
    exit(EXIT_FAILURE);
}
