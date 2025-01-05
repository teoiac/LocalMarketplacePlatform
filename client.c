#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server */
int port;

int main(int argc, char *argv[])
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
    char msg[256];             // mesajul trimis

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf("[client] Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[client] Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(argv[1]);
    /* portul de conectare */
    server.sin_port = htons(port);

    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client] Eroare la connect().\n");
        return errno;
    }
    printf("Bun Venit!\n");
    fflush(stdout);
    printf("Sintaxa utila\n ");
    fflush(stdout);
    printf("LOGIN <nume_utilizator> <parola>\n");
    fflush(stdout);
    printf("REGISTER <nume_utilizator> <parola>\n");
    fflush(stdout);

    while (1)
    {
        /* citirea mesajului */
        bzero(msg, 256);
        printf("[client] Introduceti o comanda: ");
        fflush(stdout);
        bzero(msg, 256);
        read(0, msg, 256);

        if (strncmp(msg, "QUIT", 4) == 0)
        {
            break;
        }

        /* trimiterea mesajului la server */
        if (write(sd, msg, 256) <= 0)
        {
            perror("[client] Eroare la write() spre server.\n");
            return errno;
        }
        bzero(msg, 256);
        /* citirea raspunsului dat de server (apel blocant pina cind serverul raspunde) */
        if (read(sd, msg, 256) < 0)
        {
            perror("[client] Eroare la read() de la server.\n");
            return errno;
        }
        if (strncmp(msg, "Logare cu succes!", 17) == 0)
        {
            printf("[client] Logare cu succes! Optiuni disponibile:\n");
            fflush(stdout);

            printf("ADD_PRODUCT <nume> <pret> <id_vanzator> <locatie> <cantitate disponibila> <categorie>\n");
            fflush(stdout);

            printf("VIEW_PRODUCTS \n");
            fflush(stdout);

            printf("VIEW_DETAILS <nume_produs>\n");
            fflush(stdout);

            printf("DELETE_PRODUCT <nume_produs> <id_user>\n");
            fflush(stdout);

            printf("REQUEST_ID <username> <password>\n");
            fflush(stdout);

            printf("BUY_ITEM <nume_produs> <cantitate> <id_produs> <id_user>\n");
            fflush(stdout);

            printf("CATEGORY <nume_categorie>\n");
            fflush(stdout);

            printf("MY_TRANSACTIONS <id_user>\n");
            fflush(stdout);

            printf("MENU\n");
            fflush(stdout);

            printf("LOGOUT\n");
            fflush(stdout);

            printf("QUIT\n");
            fflush(stdout);
        }
        else if(strncmp(msg, "MENU",4)==0)
        {
            printf("ADD_PRODUCT <nume> <pret> <id_vanzator> <locatie> <cantitate disponibila> <categorie>\n");
            fflush(stdout);

            printf("VIEW_PRODUCTS \n");
            fflush(stdout);

            printf("VIEW_DETAILS <nume_produs>\n");
            fflush(stdout);

            printf("DELETE_PRODUCT <nume_produs> <id_user>\n");
            fflush(stdout);

            printf("REQUEST_ID <username> <password>\n");
            fflush(stdout);

            printf("BUY_ITEM <nume_produs> <cantitate> <id_produs> <id_user>\n");
            fflush(stdout);

            printf("CATEGORY <nume_categorie>\n");
            fflush(stdout);

            printf("MY_TRANSACTIONS <id_user>\n");
            fflush(stdout);

            printf("MENU\n");
            fflush(stdout);

            printf("LOGOUT\n");
            fflush(stdout);

            printf("QUIT\n");
            fflush(stdout);
        }
        else

        /* afisam mesajul primit */
        printf("[client] Mesajul primit este: %s\n", msg);
    }

    /* inchidem conexiunea, am terminat */
    close(sd);
    return 0;
}
