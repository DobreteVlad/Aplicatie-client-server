#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
extern "C" {
#include "helpers.h"
}
#include <iostream>
#include <iterator>
#include <map>
#include <vector>
#include <sstream>
#include <math.h>
#include <iomanip>

using namespace std;

#define ADDRLEN 20

void usage(char* file)
{
    fprintf(stderr, "Usage: %s server_address server_port\n", file);
    exit(0);
}

int main(int argc, char* argv[])
{
    int sockfd, n, ret;
    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];
    fd_set temp_fds;
    fd_set read_fds;
    FD_ZERO(&temp_fds);
    FD_ZERO(&read_fds);
    int fdmax;
    char addr_port[20];

    // nu avem voie cu mai mult de 3 argumente
    if (argc < 4) {
        usage(argv[0]);
    }

    // programare defensiva pentru id-ul cu care se logheaza subscriberii
    if (strlen(argv[1]) > 10) {
        fprintf(stderr, "ID_Client has maximum 10 characters.\n");
        exit(0);
    }

    // obtinem descriptorul de fisier
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");


    int enable = 1;
    // deactivate Neagle algorithm
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &enable,
        sizeof(int));
    DIE(ret < 0, "Neagle is on");

    // completam adresa serverului, familia de adrese si portul pentru conectare
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");

    // ne conectam la port
    ret = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect");
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;

    // trimitem id-ul clientului TCP catre server
    n = send(sockfd, argv[1], strlen(argv[1]), 0);
    DIE(n < 0, "send");

    while (1) {
        // citirea comenzilor de la tastatura
        temp_fds = read_fds;
        select(fdmax + 1, &temp_fds, NULL, NULL, NULL);
        if (FD_ISSET(0, &temp_fds) != 0) {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);

            // daca primim comanda exit, inchidem clientul
            if (strncmp(buffer, "exit", 4) == 0) {

                close(sockfd);
                FD_CLR(sockfd, &read_fds);
                exit(0);
            }

            // spargem comanda in state, topic si SF
            char mybuffer[BUFLEN + 1];
            strcpy(mybuffer, buffer);
            char *state, *topic, *SF;
            state = strtok(mybuffer, " ");
            topic = strtok(NULL, " ");
            SF = strtok(NULL, "\n");

            // verificam primul parametru
            if (strcmp(state, "subscribe") != 0
                && strcmp(state, "unsubscribe") != 0) {
                cout << "First parameter must be subscribe or unsubscribe\n";
                exit(0);
            }

            // verificam al treilea parametru
            if (SF != NULL && strcmp(SF, "0") != 0 && strcmp(SF, "1") != 0
                && strcmp(state, "subscribe") == 0) {
                cout << "Third parameter must be 0 or 1\n";
                exit(0);
            }

            // nu putem avea subscribe doar cu 2 parametrii
            if (strcmp(state, "subscribe") == 0 && SF == NULL) {
                cout << "Subscribe command must have 3 parameters\n";
                exit(0);
            }

            // unsubscribe poate avea doar 2 parametrii
            if (strcmp(state, "unsubscribe") == 0 && SF != NULL) {
                cout << "Unsubscribe command must have 2 parameters\n";
                exit(0);
            }

            // daca primim comanda valida de subscribe o trimitem la server
            if (strcmp(state, "subscribe") == 0) {
                n = send(sockfd, buffer, strlen(buffer), 0);
                DIE(n < 0, "send");
                cout << "subscribed\n";
            }

            // daca primim comanda valida de unsubscribe o trimitem la server
            else if (strcmp(state, "unsubscribe") == 0) {
                n = send(sockfd, buffer, strlen(buffer), 0);
                DIE(n < 0, "send");
                cout << "unsubscribed\n";
                //break;
            }

            // afisam acest mesaj doar de siguranta, pentru a stii daca nu intra
            // unde trebuie
            else {
                cout << "The command you have written is wrong\n";
                exit(0);
            }
        }
        else {
            // mesajele primite de la server
            memset(buffer, 0, BUFLEN);
            n = recv(sockfd, buffer, BUFLEN, 0);
            // programare defensiva
            DIE(n < 0, "recv");

            // avem grija sa nu fie 2 clienti cu acelasi id
            // daca cineva incearca sa se logheze cu acelasi id,
            // inchidem clientul
            if (strcmp(buffer, ".") == 0) {
                printf("There is a client with the same ID online. "); 
                printf("Please choose another one\n");
                close(sockfd);
                exit(0);
            }
            // daca suntem abonati deja la topic
            // pentru siguranta, inchidem clientul
            if (strcmp(buffer, "Already subscribed to this topic") == 0) {
                printf("Early to this topic\n");
                close(sockfd);
                exit(0);
            }

            // daca deja nu mai suntem abonati si primim comanda din nou
            // pentru siguranta, inchidem clientul
            if (strcmp(buffer, "Already unsubscribed to this topic") == 0) {
                printf("Early to this topic\n");
                close(sockfd);
                exit(0);
            }

            // daca nu mai primim nimic de la server, inseamna ca aceasta
            // s-a inchis
            if (n == 0) {
                close(sockfd);
                printf("Server closed.\n");
                exit(0);
            }

            // prelucram mesajul primit de la udp prin server
            memset(addr_port, 0, ADDRLEN);
            n = recv(sockfd, addr_port, sizeof(addr_port), 0);

            DIE(n < 0, "recv");
            char topic[51];
            strcpy(topic, buffer);
            // trecem prin cele 3 cazrui si afisam direct mesajul
            switch (buffer[50]) {
            case 0: {
                // INT
                int payload = int((unsigned char)(buffer[52]) << 24
                    | (unsigned char)(buffer[53]) << 16
                    | (unsigned char)(buffer[54]) << 8
                    | (unsigned char)(buffer[55]));
                if (((int)buffer[51]) != 0){
                    payload *= -1;
                }
                printf("%s - %s - INT - %d\n", addr_port, topic, payload);
            } break;

            case 1: {
                // SHORT_REAL
                double payload = int((unsigned char)(buffer[51]) << 8
                    | (unsigned char)(buffer[52]));
                printf("%s - %s - SHORT_REAL - %.2f\n", addr_port, topic, (payload / 100));
            } break;

            case 2: {
                // FLOAT
                double payload = int((unsigned char)(buffer[52]) << 24
                    | (unsigned char)(buffer[53]) << 16
                    | (unsigned char)(buffer[54]) << 8
                    | (unsigned char)(buffer[55]));
                uint8_t m = buffer[56];
                for (int k = 0; k < m; k++) {
                    payload /= 10;
                }
                int sign = (int)buffer[51];
                if (sign != 0) {
                    payload *= -1;
                }

                // scapam de 0-urile in plus de dupa virgula
                int copy_payload = payload * 1000000;
                int ok = 0;
                int number_of_zeros = 0;
                while (ok == 0) {
                    if (copy_payload % 10 == 0) {
                        number_of_zeros++;
                        copy_payload /= 10;
                    }
                    else {
                        ok = 1;
                    }
                }
                printf("%s - %s - FLOAT - ", addr_port, topic);
                cout << fixed << setprecision(6 - number_of_zeros)
                    << payload << '\n';
            } break;
            case 3: {
                // STRING
                printf("%s - %s - STRING - %s\n", addr_port, topic,
                    buffer + 51);
            } break;
            }
        }
    }

    // inchidem socketul si conexiunea
    close(sockfd);

    return 0;
}
