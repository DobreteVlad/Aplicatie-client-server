DOBRETE VLAD GABRIEL
323CA
TEMA2 PC

In realizarea acestei teme, am folosit ca sursa de inspiratie laboratoarele 6,7,8.

__SERVER__

Pentru a putea tine cat mai bine evidenta fiecarei caracteristici a clientilor, am
folosit o structura de clienti cu campurile: ID, socket si status(online sau offlien).
Abonarile clientilor sunt stocate intr-un map cu cheia id si valoarea un vector de
abonari. Abonarea este reprezentata de o structura care ii retine topicul si SF-ul.
Am ales sa utilizez si un map pentru retinea mesajelor in asteptare, in functie de
clientii catre care trebuiesc livrate(id)(partea de Store and Forwarding).
Am preferat ca pentru fiecare eroare de sintaxa a comenzilor sau repetare a lor,
sa inchid clientul respectiv.

Dupa initializarea socketului de tcp si udp, caut sa vad in ce caz ma aflu cu 
mesajele primite. 

-->Serverul poate primi doar comanda exit, fapt pentru care, inchidem serverul
dar si toti clientii conectati.

-->Daca primim update-uri de la udp, separ problema in 2 cazuri:

Cazul 1: Clientii sunt deconectati insa au ales prin abonarile lor, sa retinem
updaturile. Pentru acest lucru, retinem update-urile intr-un map(offline_message)
ce colecteaza mesajele primite de la udp. Trimitem portul, IP-ul si continutul.

Cazul 2:Clientii sunt online, caz in care nu mai conteaza tipul de abonare(SF-ul).
Trimitem direct IP-ul, portul si continutul mesajului.

-->Mai departe, verific conectarea unui client TCP. La fel ca la UDP, problema
se imparte in 3 parti.

Cazul 1: Clientul se reconecteaza cu acelasi id, caz in care ii updatam
informatiile(socketul si statusul). Verific daca clientul are mesaje in asteptare.
Daca are, le trimit pe rand catre TCP si updatez map-ul cu un vector de mesaje gol
pentru a il putea folosi ulterior la o viitoare deconectare.

Cazul 2: Daca clientul incearca sa se conecteze cu un id detinut de un client
online, trimitem ack catre acesta si inchidem conexiunea cu el pentru siguranta.

Cazul 3: Clientul nu indeplineste niciunul din criteriile de mai sus, deci este
un utilizator nou. Creem o strucutra noua client pe care o initializam si o
bagam in vectorul de clienti. De asemenea, le initializam cu vectori nuli si
pozitia in mapurile de mesaje in asteptare si update-uri(topic-uri).

--> Daca serverul primeste comanda de abonare, verific mai intai sa nu fie o
repetitie(sa se dea subscribe de 2 ori la acelasi topic, caz in care trimit
ack si inchid clientul) dupa care efectuez comanda. Daca este de subscribe
adaug noua abonare, daca este de unsubscribe o sterg.

-->Inchid conexiunile

__CLIENT__

-->Verific mai intai ca id-ul sa aiba mai putin de 10 caractere.
Dupa initializarea socketului, impartim problema in 2 cazuri:

Caz 1: Citirea comenzilor de la tastatura. Ma asigur mereu ca programul sa nu
intre in bucla infinita. Trec prin toate cazurile de potentiale greseli in 
sintaxa comenzii.

Caz 2: Daca primesc mesaj de la server, vad daca este un ack pentru o petentiala
inchidere a clientului. Daca nu, inseamna ca avem un update de la UDP pe care
trebuie sa il tratam in cele 4 cazuri conform enuntului. In fiecare caz, fac
direct afisarea

-->Inchid conexiunea
