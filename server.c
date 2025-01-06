#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

/* portul folosit */
sqlite3 *db;
#define PORT 8039
extern int errno; /* eroarea returnata de unele apeluri */
pthread_mutex_t mutex;
// structura pentru a retine clientii logati
typedef struct
{
    int user_id;
    bool logged_in;
} ClientData;

ClientData *client_data_storage[250] = {NULL};

/* functie de convertire a adresei IP a clientului in sir de caractere */
char *conv_addr(struct sockaddr_in address)
{
    static char str[25];
    char port[7];

    /* adresa IP a clientului */
    strcpy(str, inet_ntoa(address.sin_addr));
    /* portul utilizat de client */
    bzero(port, 7);
    sprintf(port, ":%d", ntohs(address.sin_port));
    strcat(str, port);
    return (str);
}
// initializare baza de date

void initDB()
{
    sqlite3_open("project_db", &db);

    char *resp;
    const char *sql =
        "CREATE TABLE IF NOT EXISTS Users (id INTEGER PRIMARY KEY, username TEXT, password TEXT);"
        "CREATE TABLE IF NOT EXISTS Products (id INTEGER PRIMARY KEY, name TEXT, price REAL, location TEXT, quantity INTEGER, seller_id INTEGER, category TEXT);"
        "CREATE TABLE IF NOT EXISTS Transactions (id INTEGER PRIMARY KEY, buyer_id INTEGER, product_id INTEGER, quantity INTEGER);";

    sqlite3_exec(db, sql, 0, 0, &resp);
    printf("Baza de date initializata cu succes\n");
    fflush(stdout);
}

// inregistrare utilizatori

int registerUsers(const char *username, const char *password)
{
    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO Users (username, password) VALUES ('%s', '%s');", username, password);
    char *resp;
    sqlite3_exec(db, sql, 0, 0, &resp);
    int user_id = (int)sqlite3_last_insert_rowid(db);
    return user_id;
    //  ID pentru stergere produse
}

// functie login
bool loginUser(const char *username, const char *password)
{
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id FROM Users WHERE username='%s' AND password='%s';", username, password);
    sqlite3_stmt *stmt;
    int temp = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    temp = sqlite3_step(stmt);
    bool out = (temp == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return out;
}

// functie ajutatoare pentru a afla ID-ul in caz de il uita user-ul
int findMyID(const char *username, const char *password)
{
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id FROM Users WHERE username='%s' AND password='%s';", username, password);
    sqlite3_stmt *stmt;
    int temp = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); // adauga eroare automat prin v2
    temp = sqlite3_step(stmt);
    if (temp == SQLITE_ROW)
    {
        int user_id = sqlite3_column_int(stmt, 0); // extragem prima coloana - specifica id ului
        sqlite3_finalize(stmt);
        return user_id;
    }

    return -1;
}

// adaugare produse spre vanzare
bool addProduct(const char *name, double price, int seller_id, const char *location, int quantity, const char *category)
{
    char sql[512] = {0};
    snprintf(sql, sizeof(sql),
             "INSERT INTO Products (name, price, seller_id, location, quantity, category) VALUES ('%s', %f, %d, '%s', %d, '%s');",
             name, price, seller_id, location, quantity, category);

    char *errMsg;
    int temp = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (temp != SQLITE_OK)
    {
        printf("Eroare la adaugarea produsului: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// stergerea produselor poate fi facuta doar daca user-ul introduce si id ul propriu, doar pentru produsele sale
bool deleteProduct(const char *name, int user_id)
{
    // verificam daca exista produsul specificat si daca apartine userului
    char sql_check[512];
    snprintf(sql_check, sizeof(sql_check),
             "SELECT id FROM Products WHERE name = '%s' AND seller_id = %d;", name, user_id);
    sqlite3_stmt *stmt;

    int temp = sqlite3_prepare_v2(db, sql_check, -1, &stmt, NULL);
    temp = sqlite3_step(stmt);
    if (temp != SQLITE_ROW)
    {
        printf("Produsul nu exista sau nu apartine utilizatorului conectat.\n");
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);

    // verifica daca produsul poate fi sters
    char sql_delete[512];
    snprintf(sql_delete, sizeof(sql_delete), "DELETE FROM Products WHERE  name =  '%s' AND seller_id = %d;", name, user_id);
    char *errMsg;

    temp = sqlite3_exec(db, sql_delete, 0, 0, &errMsg);
    if (temp != SQLITE_OK)
    {
        printf("Eroare la stergerea produsului: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    printf("Produsul a fost sters cu succes.\n");
    return true;
}

// vizualizarea tuturor produselor

void viewProducts(int client_fd)
{
    const char *sql = "SELECT id, name, price FROM Products;";
    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    char buffer[4096] = {0};
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char buffer2[256]; // buffer temporar
        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        double price = sqlite3_column_double(stmt, 2);

        snprintf(buffer2, sizeof(buffer2), "ID: %d, Nume: %s, Pret: %.2f\n", id, name, price);
        strcat(buffer, buffer2);
    }

    if (strlen(buffer) == 0)
    {
        strcpy(buffer, "Nu exista produse in baza de date.\n");
    }

    write(client_fd, buffer, strlen(buffer));
    sqlite3_finalize(stmt);
}

// vizualizare detalii pentru un produs specific

void viewDetails(int client_fd, const char *product)
{
    char query[256] = {0};
    snprintf(query, sizeof(query), "SELECT * FROM Products WHERE name = '%s';", product);

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    char buffer[1024] = {0};
    int found = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        double price = sqlite3_column_double(stmt, 2);
        const char *location = (const char *)sqlite3_column_text(stmt, 4);
        int quantity = sqlite3_column_int(stmt, 5);

        snprintf(buffer, sizeof(buffer), "ID: %d, Nume: %s, Pret: %.2f, Locatie: %s, Cantitate: %d\n",
                 id, name, price, location, quantity);
        write(client_fd, buffer, strlen(buffer));
        found = 1;
    }

    if (!found)
    {
        write(client_fd, "Nu a fost gasit niciun produs cu acest nume.\n", 44);
    }

    sqlite3_finalize(stmt);
}

// afisarea tranzactiilor pentru un anumit user - se face pe baza id ului propriu
int myTransactions(int user_id, int client_fd)
{
    const char *sql = "SELECT T.id, T.quantity, P.name, P.price, P.location "
                      "FROM Transactions AS T "
                      "JOIN Products AS P ON T.product_id = P.id "
                      "WHERE T.buyer_id = ?;";

    sqlite3_stmt *stmt;
    int temp = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);

    char buffer[1024];
    buffer[0] = '\0';
    char buffer2[256];
    while ((temp = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int quantity = sqlite3_column_int(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        double price = sqlite3_column_double(stmt, 3);
        const char *location = (const char *)sqlite3_column_text(stmt, 4);
        snprintf(buffer2, sizeof(buffer2), "ID: %d, Quantity: %d, Nume: %s, Pret: %.2f, Location: %s\n",
                 id, quantity, name, price, location);

        if (strlen(buffer) + strlen(buffer2) < sizeof(buffer))
        {
            strncat(buffer, buffer2, sizeof(buffer) - strlen(buffer) - 1);
        }
        else
        {
            break;
        }
    }

    if (temp != SQLITE_DONE)
    {
        write(client_fd, "Eroare la procesarea tranzactiilor.\n", 36);
        sqlite3_finalize(stmt);
        return 1;
    }

    if (strlen(buffer) == 0)
    {
        strcpy(buffer, "Nu exista produse in baza de date.\n");
    }

    write(client_fd, buffer, strlen(buffer));
    sqlite3_finalize(stmt);
    return 0;
}

// verificare produse disponibile in functie de categorie

int checkCategory(const char *category, int client_fd)
{
    char sql_query[512];
    snprintf(sql_query, sizeof(sql_query),
             "SELECT id, name, price, location, quantity FROM Products WHERE category = '%s';", category);

    sqlite3_stmt *stmt;
    int temp = sqlite3_prepare_v2(db, sql_query, -1, &stmt, NULL);
    char buffer[4096] = {0};
    char temp_buffer[256];
    snprintf(buffer, sizeof(buffer), "Produse disponibile din categoria '%s':\n", category);
    while ((temp = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        double price = sqlite3_column_double(stmt, 2);
        const char *location = (const char *)sqlite3_column_text(stmt, 3);
        int quantity = sqlite3_column_int(stmt, 4);

        if (name == NULL)
            name = "N/A";
        if (location == NULL)
            location = "N/A";

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "ID: %d, Name: %s, Price: %.2f, Location: %s, Quantity: %d\n",
                 id, name, price, location, quantity);

        strncat(buffer, temp_buffer, sizeof(buffer) - strlen(buffer) - 1);
    }

    if (strlen(buffer) == strlen("Produse disponibile din categoria '':\n"))
    {
        snprintf(temp_buffer, sizeof(temp_buffer), "Nu au fost gasite produse in categoria '%s'.\n", category);
        strncat(buffer, temp_buffer, sizeof(buffer) - strlen(buffer) - 1);
    }

    write(client_fd, buffer, strlen(buffer));
    sqlite3_finalize(stmt);
    return 0;
}

// cumparare produs si inregistrare tranzactie

int buyItem(const char *product_name, int quantity, int product_ID, int buyer_ID, int client_fd)
{
    char sql_query[512];

    // verfica disponibilitatea produsului

    snprintf(sql_query, sizeof(sql_query), "SELECT id, quantity FROM Products WHERE id = %d;", product_ID);

    sqlite3_stmt *stmt;
    int temp = sqlite3_prepare_v2(db, sql_query, -1, &stmt, NULL);
    int product_db_id = -1;
    int available_quantity = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        product_db_id = sqlite3_column_int(stmt, 0);
        available_quantity = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (product_db_id == -1 || available_quantity < quantity)
    {
        write(client_fd, "Produsul nu a fost gasit sau cantitatea este insuficienta.\n", 59);
        return 1;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    snprintf(sql_query, sizeof(sql_query), "UPDATE Products SET quantity = quantity - %d WHERE id = %d;", quantity, product_ID);
    temp = sqlite3_exec(db, sql_query, NULL, NULL, NULL);
    if (temp != SQLITE_OK)
    {
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", 0, 0, 0); // rollback just in case
        fprintf(stderr, "eraore update cantitate noua: %s\n", sqlite3_errmsg(db));
        write(client_fd, "Eroare la update cantitate noua.\n", 33);
        return 1;
    }

    snprintf(sql_query, sizeof(sql_query), "INSERT INTO Transactions (buyer_id, product_id, quantity) VALUES (%d, %d, %d);", buyer_ID, product_ID, quantity);
    temp = sqlite3_exec(db, sql_query, NULL, NULL, NULL);
    sqlite3_exec(db, "COMMIT TRANSACTION;", 0, 0, 0); // commit tranzacite
    write(client_fd, "Produs cumparat cu succes!.\n", 28);
    return 0;
}

bool processRequest(int client_fd)
{
    if (client_data_storage[client_fd] == NULL)
    {
        client_data_storage[client_fd] = (ClientData *)calloc(1, sizeof(ClientData));
    }

    ClientData *client_data = client_data_storage[client_fd];

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    int bytes = read(client_fd, buffer, 256);
    if (bytes <= 0)
    {
        perror("Eroare la citire de la client.");
        return true; // deconectam clientul
    }

    buffer[bytes] = '\0';

    // pt quit
    if (strncmp(buffer, "QUIT", 4) == 0)
    {
        write(client_fd, "Deconectare...\n", 15);
        free(client_data_storage[client_fd]);
        client_data_storage[client_fd] = NULL;
        return true;
    }
    if (strncmp(buffer, "LOGOUT", 6) == 0)
    {
        if (client_data->logged_in)
        {
            client_data->logged_in = false;
            client_data->user_id = -1;
            write(client_fd, "Utilizator delogat!\n", 20);
        }
        else
        {
            write(client_fd, "Nu sunteti logat!\n", 18);
        }
        return false;
    }
    if (strncmp(buffer, "REGISTER", 8) == 0)
    {
        char username[50], password[50];
        sscanf(buffer, "REGISTER %s %s", username, password);

        snprintf(buffer, sizeof(buffer), "User creat! Id-ul dvs este: %d\n", registerUsers(username, password));
        write(client_fd, buffer, strlen(buffer) + 1);
    }
    else if (strncmp(buffer, "LOGIN", 5) == 0)
    {
        char username[50], password[50];
        int parsed = sscanf(buffer, "LOGIN %49s %49s", username, password); // Limit input

        if (parsed != 2)
        {
            write(client_fd, "SINTAXA : LOGIN <username> <parola>\n", 36);
            return false;
        }

        if (client_data->logged_in == false)
        {
            if (loginUser(username, password))
            {
                const char *query = "SELECT id FROM Users WHERE username = ?;";
                sqlite3_stmt *stmt;
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK)
                {
                    write(client_fd, "EROARE DB.\n", 11);
                    return true;
                }

                sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

                int rc = sqlite3_step(stmt);

                if (rc == SQLITE_ROW)
                {
                    client_data->user_id = sqlite3_column_int(stmt, 0);
                    client_data->logged_in = true;
                    write(client_fd, "Logare cu succes!\n", 17);
                }
                else
                {
                    write(client_fd, "Userul nu a fost gasit.\n", 24);
                }

                sqlite3_finalize(stmt);
            }
            else
            {
                write(client_fd, "Nume de utilizator sau parola incorecta\n", 40);
            }
        }
        else
        {
            write(client_fd, "Sunteti deja logat.\n", 20);
        }
    }
    else if (client_data->logged_in == true)
    {
        if (strncmp(buffer, "ADD_PRODUCT", 11) == 0)
        {
            char name[50], location[50], category[50];
            double price = 0.0;
            int seller_id = 0, quantity = 0;
            sscanf(buffer, "ADD_PRODUCT %49s %lf %d %49s %d %49s", name, &price, &seller_id, location, &quantity, category);
            if (addProduct(name, price, seller_id, location, quantity, category))
            {
                write(client_fd, "Produs adaugat.\n", 17);
            }
            else
            {
                write(client_fd, "Eroare la adaugare.\n", 21);
            }
        }
        else if (strncmp(buffer, "VIEW_PRODUCTS", 13) == 0)
        {
            viewProducts(client_fd);
        }
        else if (strncmp(buffer, "REQUEST_ID", 10) == 0)
        {
            char username[50], password[50];
            sscanf(buffer, "REQUEST_ID %s %s", username, password);
            if (findMyID(username, password) != -1)
            {
                char response[100] = {0};
                snprintf(response, sizeof(response), "ID-ul dumneavoastra este : %d\n", findMyID(username, password));
                write(client_fd, response, strlen(response) + 1);
            }
            else
            {
                write(client_fd, "Eroare la request\n", 19);
            }
        }
        else if (strncmp(buffer, "DELETE_PRODUCT", 14) == 0)
        {
            char product[30];
            int user_id;
            if (sscanf(buffer, "DELETE_PRODUCT %s %d", product, &user_id) != 2)
            {
                write(client_fd, "Comanda DELETE_PRODUCT invalida.\n", 33);
                return false;
            }

            if (deleteProduct(product, user_id))
                write(client_fd, "Produs sters cu succes.\n", 24);
            else
                write(client_fd, "Produsul nu a putut fi sters.\n", 30);
        }
        else if (strncmp(buffer, "VIEW_DETAILS", 12) == 0)
        {
            char product[30];
            sscanf(buffer, "VIEW_DETAILS %s", product);
            viewDetails(client_fd, product);
        }
        else if (strncmp(buffer, "CATEGORY", 8) == 0)
        {
            char category[30], response[200];
            sscanf(buffer, "CATEGORY %s", category);
            checkCategory(category, client_fd);
            write(client_fd, response, strlen(response) + 1);
        }
        else if (strncmp(buffer, "MY_TRANSACTIONS", 15) == 0)
        {
            int user_id;
            if (sscanf(buffer + 15, "%d", &user_id) != 1)
            {
                write(client_fd, "Sintaxa incorecta, incercati :  MY_TRANSACTIONS <user_id>\n", 58);
            }
            myTransactions(user_id, client_fd);
        }
        else if (strncmp(buffer, "BUY_ITEM", 8) == 0)
        {
            char product[50];
            int quantity, product_id, user_id;
            sscanf(buffer, "BUY_ITEM %s %d %d %d", product, &quantity, &product_id, &user_id);
            buyItem(product, quantity, product_id, user_id, client_fd);
        }
        else if (strncmp(buffer, "MENU", 4) == 0)
        {
            write(client_fd, "MENU", 5);
        }
        else
        {
            write(client_fd, "Comanda necunoscuta.\n", 21);
        }
    }
    else
    {

        write(client_fd, "Nu sunteti logat!\n", 18);
    }
    return false;
}

/* fct pentru fiecare thread -client*/
void *treat(void *arg)
{
    int client_fd = *((int *)arg); // castare la int - arg contine adresa unui client_fd
    free(arg);
    pthread_mutex_lock(&mutex);
    printf("Client conectat - descriptor : %d\n", client_fd);
    pthread_mutex_unlock(&mutex);

    while (1)
    {
        if (processRequest(client_fd))
        {
            pthread_mutex_lock(&mutex);
            printf("[server] S-a deconectat clientul cu descriptorul %d.\n", client_fd);
            fflush(stdout);
            pthread_mutex_unlock(&mutex);
            close(client_fd);
            break;
        }
    }
    return NULL;
}

/* programul */
int main()
{
    struct sockaddr_in server; /* structurile pentru server si clienti */
    struct sockaddr_in from;
    int sd, client; /* descriptori de socket */
    int optval = 1; /* optiune folosita pentru setsockopt()*/
    socklen_t len;  /* lungimea structurii sockaddr_in */
    pthread_t thread_id;
    pthread_mutex_init(&mutex, NULL);

    initDB();
    /* creare socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Eroare la socket().\n");
        return errno;
    }

    /*setam pentru socket optiunea SO_REUSEADDR */
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    /* pregatim structurile de date */
    bzero(&server, sizeof(server));

    /* umplem structura folosita de server */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    /* atasam socketul */
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server] Eroare la bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, 5) == -1)
    {
        perror("[server] Eroare la listen().\n");
        return errno;
    }

    printf("[server] Asteptam la portul %d...\n", PORT);
    fflush(stdout);

    /* bucla principala*/
    while (1)
    {
        len = sizeof(from);
        bzero(&from, sizeof(from));

        client = accept(sd, (struct sockaddr *)&from, &len);
        if (client < 0)
        {
            perror("[server] Eroare la accept().\n");
            continue;
        }

        // creaza un thread pentru fiecare client
        int *client_ptr = malloc(4);
        *client_ptr = client;
        if (pthread_create(&thread_id, NULL, treat, (void *)client_ptr) != 0)
        {
            perror("[server] Eroare la crearea thread-ului.\n");
            continue;
        }

        pthread_detach(thread_id); // se elibereaza thread-ul automat
    }
    pthread_mutex_destroy(&mutex);
    return 0;
}
