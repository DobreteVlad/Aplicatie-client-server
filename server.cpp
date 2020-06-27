#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
extern "C" {
#include "helpers.h"
}
#include <iostream>
#include <iterator>
#include <map>
#include <vector>
#include <sstream>
#include <math.h>

using namespace std;

void usage(char* file)
{
    fprintf(stderr, "Usage: %s server_port\n", file);
    exit(0);
}

// strucutra pentru a retine topicul abonarii si SF-ul
struct Client_topic {
    char client_topic[51];
    int SF;
};

// structura pentru clienti
// retinem id, statusul(online sau offline) si socketul
struct Client {
    int id;
    int status;
    int socket_fd;
};

int main(int argc, char* argv[])
{
    int tcp_sockfd, newsockfd, portno, udp_sockfd;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr, udp_cli_addr;
    int n, i, ret;
    socklen_t clilen;
    int fdmax;
    fd_set read_fds;
    fd_set tmp_fds;

    // vector de clienti
    vector<Client> myclients;

    // map pentru retinerea multimii de mesaje in functie de id
    map<int, vector<Client_topic> > mymap;

    // map pentru retinerea multimii de mesaje in asteptare in functie de id
    map<int, vector<char*> > offline_message;

    // avem voie doar cu comanda de exit
    if (argc < 2) {
        usage(argv[0]);
    }

    // stergem multimea de descriptori de fisiere set
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // obtinem descriptorul de fisier pentru tcp
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "socket tcp");

    // obtinem descriptorul de fisier pentru udp
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "socket udp");

    // verificam portul dat ca parametru
    portno = atoi(argv[1]);
    DIE(portno == 0, "atoi");

    // completam adresa serverului, familia de adrese si portul pt. conectare
    memset((char*)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // asociem port
    ret = bind(tcp_sockfd, (struct sockaddr*)&serv_addr,
    	sizeof(struct sockaddr));
    DIE(ret < 0, "bind");

    // asociem port
    ret = bind(udp_sockfd, (struct sockaddr*)&serv_addr,
    	sizeof(struct sockaddr));
    DIE(ret < 0, "bind_udp");

    // "ascultam" pe acest socket
    ret = listen(tcp_sockfd, MAX_CLIENTS);
    DIE(ret < 0, "listen");

    int flag = 1;
    ret = setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &flag ,sizeof(int));
    DIE(ret < 0, "reuse address failed");

    ret = setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    DIE(ret < 0, "reuse address failed");

    // adaugam descriptorii in multimea de descriptori
    FD_SET(tcp_sockfd, &read_fds);
    FD_SET(udp_sockfd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    fdmax = max(tcp_sockfd, udp_sockfd) + 1;

    while (1) {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
            	// cazul udp
                if (i == udp_sockfd) {
                    clilen = sizeof(udp_cli_addr);
                    memset(buffer, 0, BUFLEN);

                    // primim mesajul
                    n = recvfrom(udp_sockfd, (char*)buffer, BUFLEN, 0,
                    	(struct sockaddr*) &udp_cli_addr, &clilen);

                    // "spargem" mesajul(ne va trebui topicul)
                    char* topic;
                    topic = strtok(buffer, " ");

                    // cautam prin abonarile clientilor
                    // separam problema in 2 cazuri
                    // cazul 1: clientii TCP sunt conectati, deci le trimitem 
                    // mesajele de la udp
                    for (int k = 0; k < myclients.size(); k++) {
                        for (auto subbed_topic
                        	: mymap.find(myclients[k].id)->second) {
                            if (!strcmp(subbed_topic.client_topic, topic)
                            	&& myclients[k].status == 1) {
                                char addr_port[20] = "";
                                strncat(addr_port,
                                	inet_ntoa(udp_cli_addr.sin_addr),
                                	sizeof(inet_ntoa(udp_cli_addr.sin_addr)));
                                strcat(addr_port, ":");
                                strncat(addr_port,
                                	to_string(ntohs(udp_cli_addr.sin_port)).c_str(),
                                	sizeof(to_string(ntohs(udp_cli_addr.sin_port)).c_str()));

                                // trimitem update-urile catre clientii TCP
                                n = send(myclients[k].socket_fd, buffer,
                                	sizeof(buffer), 0);
                                DIE(n < 0, "send");
                                n = send(myclients[k].socket_fd, addr_port,
                                	sizeof(addr_port), 0);
                                DIE(n < 0, "send");
                            }

                            // cazul 2: clientul TCP este deconectat, dar a 
                            // selectat optiunea de pastrare a mesajelor
                            // le retinem intr-un map(cheia = id, valoarea = multimea de mesaje pastrate)
                            else if (!strcmp(subbed_topic.client_topic, topic)
                            	&& myclients[k].status == 0
                            	&& subbed_topic.SF == 1) {
                                char addr_port[20] = "";
                                strncat(addr_port,
                                	inet_ntoa(udp_cli_addr.sin_addr),
                                	sizeof(inet_ntoa(udp_cli_addr.sin_addr)));
                                strcat(addr_port, ":");
                                strncat(addr_port,
                                	to_string(ntohs(udp_cli_addr.sin_port)).c_str(),
                                	sizeof(to_string(ntohs(udp_cli_addr.sin_port)).c_str()));
                                char* mybuffer = (char*)malloc((BUFLEN + 1)
                                	* sizeof(char));
                                char* myaddr = (char*)malloc(20 * sizeof(char));
                                memcpy(myaddr, addr_port, 20);
                                memcpy(mybuffer, buffer, (BUFLEN + 1));
                                vector<char*> auxx
                                	= offline_message.find(myclients[k].id)->second;
                                auxx.push_back(mybuffer);
                                auxx.push_back(myaddr);
                                offline_message.at(myclients[k].id) = auxx;
                            }
                        }
                    }
                }

                // daca primim de la tastatura comanda exit pentru server
                // inchidem si serverul si clientii
                else if (i == STDIN_FILENO) {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);
                    if (strcmp(buffer, "exit\n") == 0) {
                        close(tcp_sockfd);
                        close(udp_sockfd);
                        exit(0);
                    }
                }

                // cazul TCP
                else if (i == tcp_sockfd) {

                	// serverul accepta cererea de conexiune venita de pe 
                	// socketul inactiv
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(tcp_sockfd,
                    	(struct sockaddr*)&cli_addr, &clilen);
                    DIE(newsockfd < 0, "accept");

                    // adaugam noul socket la multimea de descriptori
                    FD_SET(newsockfd, &read_fds);
                    if (newsockfd > fdmax) {
                        fdmax = newsockfd;
                    }
                    // primim id-ul clientului
                    memset(buffer, 0, BUFLEN);
                    n = recv(newsockfd, buffer, sizeof(buffer), 0);

                    DIE(n < 0, "recv");
                    bool client_exists = false;

                    // verificam daca clientul este nou sau se reconecteaza
                    for (int k = 0; k < myclients.size(); k++) {
                        int ok = 0;
                        // daca clientul a mai fost conectat
                        if (myclients[k].id == atoi(buffer)
                        	&& myclients[k].status == 0) {
                            ok = 1;
                            cout << "Client " << atoi(buffer)
                            << " reconnected from " << inet_ntoa(cli_addr.sin_addr)
                            << ":" << ntohs(cli_addr.sin_port) << '\n';

                            // improspatam caracteristicile acestuia
                            myclients[k].socket_fd = newsockfd;
                            myclients[k].status = 1;
                            client_exists = true;

                            // verificam daca are mesaje in asteptare
                            vector<char*> auxx
                            	= offline_message.find(myclients[k].id)->second;

                            while (!auxx.empty()) {
                            	// daca are mesaje in asteptare, le trimitem catre client in ordine
                                n = send(newsockfd, auxx.front(),
                                	sizeof(buffer), 0);
                                DIE(n < 0, "send");
                                auxx.erase(auxx.begin());
                                n = send(newsockfd, auxx.front(), 20, 0);
                                DIE(n < 0, "send");
                                auxx.erase(auxx.begin());
                            }
                            offline_message.at(myclients[k].id) = auxx;
                            break;
                        }

                        // daca clientul incearca sa foloseaca acelasi id cu un client online
                        // trimitem ack catre client si il inchidem
                        else if (myclients[k].id == atoi(buffer)
                        	&& myclients[k].status == 1 && ok == 0) {
                            cout << "Someone tryed to connect with the same ID as client "
                        		<< atoi(buffer) << '\n';
                            strcpy(buffer, ".");
                            send(newsockfd, buffer, sizeof(buffer), 0);
                            client_exists = true;
                            break;
                        }
                    }

                    // daca clientul este nou, il adaugam in vectorul de clienti
                    if (client_exists == false) {
                        cout << "New client " << atoi(buffer)
                        << " connected from " << inet_ntoa(cli_addr.sin_addr)
                        << ":" << ntohs(cli_addr.sin_port) << '\n';
                        Client new_client;
                        new_client.id = atoi(buffer);
                        new_client.status = 1;
                        new_client.socket_fd = newsockfd;
                        myclients.push_back(new_client);
                        vector<Client_topic> aux;
                        vector<char*> aux1;

                        // actualizam si mapurile pentru mesaje retinute si 
                        // abonari cu vectori nuli
                        offline_message.insert(pair<int, vector<char*> >(atoi(buffer), aux1));
                        mymap.insert(pair<int, vector<Client_topic> >(atoi(buffer), aux));
                    }
                }

                // daca serverul primeste comanda de abonare
                else {
                	// primim comanda
                    memset(buffer, 0, BUFLEN);
                    n = recv(i, buffer, sizeof(buffer), 0);
                    DIE(n < 0, "recv");

                    // daca nu primim raspuns de la client
                    // inchidem conexiunea
                    if (n == 0) {
                    disconnect:
                        for (int k = 0; k < myclients.size(); k++) {
                            if (myclients[k].socket_fd == i) {
                                myclients[k].status = 0;
                                printf("Client %d disconnected\n", myclients[k].id);
                                close(i);
                                FD_CLR(i, &read_fds);
                                break;
                            }
                        }
                    }
                    else {
                    	// parsam mesajul
                        char mybuffer[BUFLEN];
                        strcpy(mybuffer, buffer);
                        char *state, *topic, *SF;
                        state = strtok(mybuffer, " ");
                        if (strcmp(state, "subscribe") == 0) {
                            topic = strtok(NULL, " ");
                            SF = strtok(NULL, "\n");
                            int ok = 0;

                            // verificam daca nu exista deja o abonare
                            for (auto my_client : myclients) {
                                if (my_client.socket_fd == i) {
                                    for (auto existent_topic : mymap.find(my_client.id)->second) {
                                        if (strcmp(topic, existent_topic.client_topic) == 0) {
                                            ok = 1;
                                            strcpy(buffer, "Already subscribed to this topic");
                                            send(i, buffer, sizeof(buffer), 0);
                                            close(i);
                                            FD_CLR(i, &read_fds);
                                            goto disconnect;
                                        }
                                    }
                                }
                                if (ok == 1) {
                                    break;
                                }
                            }

                            // daca nu exista abonare, creez una si o adaug in map
                            if (ok == 0) {
                                for (auto my_client : myclients) {
                                    if (my_client.socket_fd == i) {
                                        Client_topic new_topic;
                                        strncpy(new_topic.client_topic, topic, 50);
                                        new_topic.SF = atoi(SF);
                                        vector<Client_topic> aux
                                        = mymap.find(my_client.id)->second;
                                        aux.push_back(new_topic);
                                        mymap.at(my_client.id) = aux;
                                    }
                                }
                            }
                        }

                        if (strcmp(state, "unsubscribe") == 0) {
                            topic = strtok(NULL, "\n");
                            int var = -1;

                            // dezabonam clientul stergand abonarea din map
                            for (Client my_client : myclients) {
                                if (my_client.socket_fd == i) {
                                    vector<Client_topic> aux
                                    = mymap.find(my_client.id)->second;
                                    for (int k = 0; k < aux.size(); k++) {
                                        if (strcmp(aux[k].client_topic, topic) == 0) {
                                            //fprintf(stderr, "DA\n" );
                                            var = k;
                                            aux.erase(aux.begin() + var);
                                            mymap.at(my_client.id) = aux;
                                            break;
                                        }
                                    }
                                }
                            }

                            // daca nu a fost efectuata nici-o dezabonare
                            // clientul era deja dezabonat
                            // trimitem ack
                            // inchidem conexiunea
                            if (var == -1) {
                                strcpy(buffer, "Already unsubscribed to this topic");
                                send(i, buffer, sizeof(buffer), 0);
                                close(i);
                                FD_CLR(i, &read_fds);
                                goto disconnect;
                            }
                        }
                    }
                }
            }
        }
    }
    // inchidem conexiunea cu clientii
exit1:
    close(tcp_sockfd);
    close(udp_sockfd);
    return 0;
}