#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// ---------- COSTANTI ----------
#define MAX_ARG_LENGTH 20
#define MAX_CMD_LENGTH 6
#define MAX_NAME_LENGTH 20
#define MAX_DESCRIPTION_LENGTH 1024
#define REQ_SIZE 48

// ---------- DICHIARAZIONI FUNZIONI ----------
void msg_comandi();
int crea_connessione(int port);
void check(int r);
void msg_comandi_game();

void login(int narg, char* utente, char* password);
void logout();
void signup(int narg, char* utente, char* password);
void rooms();
void game(int narg, char* id);
void stampa_dati_partita();

int look_cmd(int narg, char* arg1);
int take_cmd(int narg, char* arg1);
int use_cmd(int narg, char* arg1, char* arg2);
int objs_cmd();
void end_cmd();
int deposit_cmd(int narg, char* arg1);

void inizializzazione_messaggistica();
void connessione_messaggistica();
void msg_player();
int gestione_messaggi();
void notifica_player(int tipo);

// ---------- VARIABILI GLOBALI ----------
int socket_desc;
int msg_desc;
int listener;
int logged = 0;
int msg_port;
int nplayer;

fd_set master;
int fd_max;
// ---------- MAIN ----------
int main(int argc, char *argv[]){
	int fine = 0;
	int port, narg;
	char cmd[REQ_SIZE];
	char arg1[MAX_CMD_LENGTH];
	char arg2[MAX_ARG_LENGTH];
	char arg3[MAX_ARG_LENGTH];


	if(argc != 2){
		printf("argomento non corretto\n");
		exit(-1);
	}
	msg_port = atoi(argv[1]);
	port = 4242;
	
	//connessione al server
	socket_desc = crea_connessione(port);

	msg_comandi();
	do{
		printf("> ");
		fgets(cmd, sizeof(cmd), stdin);
		narg = sscanf(cmd, "%s %s %s", arg1, arg2, arg3);
		
		//comando login
		if(strncmp(arg1, "login", 5) == 0){
			login(narg - 1, arg2, arg3);
			continue;
		}
		//comando signup
		if(strncmp(arg1, "signup", 6) == 0){
			signup(narg - 1, arg2, arg3);
			continue;
		}
		//comando logout
		if(strncmp(arg1, "logout", 6) == 0){
			logout();
		}
		//comando rooms
		if(strncmp(arg1, "rooms", 5) == 0){
			rooms();
			continue;
		}

		//comando start
		if(strncmp(arg1, "start", 5) == 0){
			game(narg - 1, arg2);
			continue;
		}

		printf("comando sconosciuto\n");
		msg_comandi();
	}while(fine == 0);

	return 0;
}

// -----------------------------------------
// |          FUNZIONI DI UTILITA          |
// -----------------------------------------

//stampa a video dei comandi disponibili
void msg_comandi(){
	printf("\n\n********** ESCAPE ROOM **********\n");
	printf("Digita un comando:\n\n");
	printf("1) login nome_utente password --> fai il login al tuo account\n");
	printf("2) signup nome_utente password --> crea un account\n");
	printf("3) logout --> chiudi il gioco\n");
	printf("4) rooms --> visualizza la lista di rooms disponibili\n");
	printf("5) start id --> inizia una partita nella room avente tale id\n");
	printf("********************************\n");
}

//creazione di una connessione
int crea_connessione(int port){
	int socket_d, esito;

	socket_d = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server;

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

	esito = connect(socket_d, (struct sockaddr*) &server, sizeof(server));
	if(port == 4242){
		check(esito);
		printf("connessione con il server stabilita\n");
	}
	return socket_d;
}

//controllo della presenza di errori
void check(int r){
	if(r < 0){
		perror("errore:\n");
		close(socket_desc);
		exit(-1);
	}
}

//stampa a video dei comandi disponibili in partita
void msg_comandi_game(){
	printf("\n\n********** PARTITA **********\n");
	printf("1) look [location | object] --> visualizza una breve descrizione\n");
	printf("2) take object --> prendi l'oggetto\n");
	printf("3) use object1 [object2] --> usa gli oggetti\n");
	printf("4) objs --> visualizza gli oggetti raccolti\n");
	printf("5) dep object --> deposita un oggetto dall'inventario\n");
	printf("6) msg --> manda un messaggio all'altro giocatore\n");
	printf("7) end --> termina il gioco\n");
	printf("*****************************\n");
}

// -----------------------------------------
// |          FUNZIONI PRINCIPALI          |
// -----------------------------------------

//comunicazione del login con il server
void login(int narg, char* utente, char* password){
	char msg[REQ_SIZE];
	char risp[2];

	memset((void*) msg, 0, REQ_SIZE);

	//controllo formato comando
	if(narg != 2){
		printf("comando errato\n");
		return;
	}
	//controllo se il login è già avvenuto
	if(logged == 1){
		printf("il login è stato già effettuato");
		return;
	}

	//composizione richiesta
	strcpy(msg, "login ");
	strcat(msg, utente);
	strcat(msg, " ");
	strcat(msg, password);

	check(send(socket_desc, msg, REQ_SIZE, 0));
	check(recv(socket_desc, &risp[0], 2, 0));

	if(strncmp(risp, "OK", 2) == 0){
		char port[MAX_NAME_LENGTH];

		printf("login effettuato correttamente!\n");
		logged = 1;

		//comunico la porta per i messaggi
		memset(msg, 0, sizeof(msg));
		memset(port, 0, sizeof(port));
		sprintf(port, "%d", msg_port);
		strcpy(msg, "setp ");
		strcat(msg, port);
		check(send(socket_desc, msg, sizeof(msg), 0));

		return;
	}
	printf("login errato\n");
}

//logout dall'account e chiusura gioco e connessione
void logout(){
	char req[REQ_SIZE];

	memset(req, 0, sizeof(req));
	strcpy(req, "logout");
	check(send(socket_desc, req, sizeof(req), 0));
	close(socket_desc);
	exit(0);
}

//comunicazione della creazione di un nuovo account al server
void signup(int narg, char* utente, char* password){
	char msg[REQ_SIZE];
	char risp[2];

	if(narg != 2){
		printf("comando errato\n");
		return;
	}

	//composizione richiesta
	memset(msg, 0, sizeof(msg));
	strcpy(msg, "signup ");
	strcat(msg, utente);
	strcat(msg, " ");
	strcat(msg, password);

	check(send(socket_desc, msg, sizeof(msg), 0));
	check(recv(socket_desc, risp, sizeof(risp), 0));

	if(strcmp(risp, "OK") == 0){
		printf("register effettuato correttamente!\n");
		return;
	}
	printf("register errato\n");
}

//recupero della lista delle stanze disponibili dal server
void rooms(){
	char buf[REQ_SIZE];
	char risp[MAX_NAME_LENGTH];
	int i;
	uint8_t n;

	//composizione richiesta
	memset(buf, 0, sizeof(buf));
	printf("lista rooms disponibili:\n");
	strcpy(buf, "rooms");

	check(send(socket_desc, buf, sizeof(buf), 0));
	check(recv(socket_desc, &n, sizeof(uint8_t), 0));

	//ricezione delle n stanze
	for(i = 0; i < n; i++){
		memset(risp, 0, sizeof(risp));
		check(recv(socket_desc, risp, sizeof(risp), 0));
		printf("%d) %s id: %d\n", i + 1,  risp, i);
	}
}

// -------------------------------------------
// |         PARTE DEDICATA AL GIOCO         |
// -------------------------------------------

// ---------- FUNZIONE GESTIONE COMANDI DI GIOCO ----------
void game(int n, char* id){
	int fine = 0, narg, player_desc = -1;
	char cmdline[MAX_CMD_LENGTH + MAX_ARG_LENGTH * 2 + 2];
	char cmd[MAX_CMD_LENGTH];
	char arg1[MAX_ARG_LENGTH];
	char arg2[MAX_ARG_LENGTH];
	char buf[REQ_SIZE];
	char risp[2];
	fd_set read_fds;

	//controllo login
	if(logged != 1){
		printf("E' necessario effettuare il login prima di avviare una partita\n");
		return;
	}

	msg_desc = -1;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(STDIN_FILENO, &master);
	fd_max = STDIN_FILENO;

	//manda richiesta inizio partita
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "start ");
	strcat(buf, id);

	//spedizione richiesta e controllo risposta
	check(send(socket_desc, buf, sizeof(buf), 0));
	check(recv(socket_desc, risp, sizeof(risp), 0));
	if(strncmp(risp, "GO", 2) == 0){
		nplayer = 1;
		printf("partita creata, sei il giocatore 1\n");
		inizializzazione_messaggistica();
	}
	else if(strncmp(risp, "G2", 2) == 0){
		nplayer = 2;
		printf("partita trovata, sei il giocatore 2\n");
		connessione_messaggistica();
	}
	else{
		printf("errore\n");
		return;
	}

	msg_comandi_game();

	while(fine == 0){
		read_fds = master;
		select(fd_max + 1, &read_fds, NULL, NULL, NULL);
		if(FD_ISSET(msg_desc, &read_fds)){
			fine = gestione_messaggi();
			if(fine == 1)
				break;
			continue;
		}
		if(listener >= 0 && FD_ISSET(listener, &read_fds) && msg_desc < 0){
			socklen_t csize;
			struct sockaddr_in peer;

			printf("GIOCATORE 2 PARTECIPA ALLA PARTITA\n");
			msg_desc = accept(listener, (struct sockaddr*) &peer, &csize);
			FD_SET(msg_desc, &master);
			if(fd_max < msg_desc)
				fd_max = msg_desc;
			close(listener);
			FD_CLR(listener, &master);
			listener = -1;
		}
		if(!FD_ISSET(STDIN_FILENO, &read_fds))
			continue;

		memset(cmdline, 0, sizeof(cmdline));
		memset(cmd, 0, sizeof(cmd));
		memset(arg1, 0, sizeof(arg1));
		memset(arg2, 0, sizeof(arg2));
		fgets(cmdline, MAX_CMD_LENGTH + MAX_ARG_LENGTH * 2 + 2, stdin);
		narg = sscanf(cmdline, "%s %s %s", cmd, arg1, arg2);

		//le funzioni di gestione dei comandi ritornano 1 se la partita deve finire, 0 altrimenti

		//comando look
		if(strcmp(cmd, "look") == 0){
			fine = look_cmd(narg - 1, arg1);
			stampa_dati_partita();
			continue;
		}

		//comando take
		if(strcmp(cmd, "take") == 0){
			fine = take_cmd(narg - 1, arg1);
			stampa_dati_partita();
			continue;
		}

		//comando use
		if(strcmp(cmd, "use") == 0){
			fine = use_cmd(narg - 1, arg1, arg2);
			stampa_dati_partita();
			continue;
		}

		//comando objs
		if(strcmp(cmd, "objs") == 0){
			fine = objs_cmd();
			stampa_dati_partita();
			continue;
		}

		//comando end
		if(strcmp(cmd, "end") == 0){
			end_cmd();
			fine = 1;
			continue;
		}

		//comando dep
		if(strcmp(cmd, "dep") == 0){
			fine = deposit_cmd(narg - 1, arg1);
			continue;
		}

		//comando msg
		if(strcmp(cmd, "msg") == 0){
			msg_player();
			continue;
		}
		
		printf("comando sconosciuto\n");
		msg_comandi_game();
	}
	msg_comandi();
	
	if(msg_desc >= 0){
		close(msg_desc);
		msg_desc = -1;
	}
}

// ---------- DATI PARTITA ----------
//stampa a video tempo rimasto e tokens raccolti
void stampa_dati_partita(){
	uint16_t tempo, t_taken, t_max;
	char req[REQ_SIZE];

	strcpy(req, "time");
	check(send(socket_desc, req, sizeof(req), 0));
	check(recv(socket_desc, &tempo, sizeof(tempo), 0));

	printf("----- INFORMAZIONI -----\n");
	printf("TEMPO RIMASTO: %d secondi\n", tempo);

	memset(req, 0, sizeof(req));
	strcpy(req, "tokens");
	check(send(socket_desc, req, sizeof(req), 0));
	check(recv(socket_desc, &t_taken, sizeof(t_taken), 0));
	check(recv(socket_desc, &t_max, sizeof(t_max), 0));

	printf("TOKENS RACCOLTI %d su %d\n", t_taken, t_max);
	printf("------------------------\n\n");
}

// ---------- LOOK ----------
//richiesta al server della descrizione di una data location
int look_cmd(int narg, char* arg1){
	char msg[REQ_SIZE];
	char risp[2 + MAX_DESCRIPTION_LENGTH];

	memset(msg, 0, sizeof(msg));
	memset(risp, 0, sizeof(risp));

	//composizione richiesta
	strcpy(msg, "look ");
	//se la location non è specificata allora si richiede la location start
	if(narg == 0)
		strcat(msg, "start");
	else
		strcat(msg, arg1);
	check(send(socket_desc, msg, sizeof(msg), 0));
	check(recv(socket_desc, risp, sizeof(risp), 0));

	if(strncmp(risp, "OK", 2) == 0){
		printf("%s\n", &risp[2]);
	}
	else if(strncmp(risp, "TS", 2) == 0){
		printf("TEMPO SCADUTO\n");
		notifica_player(2);
		return 1;
	}
	else
		printf("object o location inesistente\n");
	
	return 0;
}

// ---------- TAKE ----------
//comunica al server la volontà di raccogliere un oggetto
int take_cmd(int narg, char* arg1){
	char msg[REQ_SIZE];
	char risp[2];

	//controllo formato
	if(narg != 1){
		printf("CORRETTO UTILIZZO DI TAKE: take <object>\n");
		return 0;
	}

	//composizione richiesta
	memset(msg, 0, sizeof(msg));
	strcpy(msg, "take ");
	strcat(msg, arg1);
	send(socket_desc, (void*) msg, sizeof(msg), 0);
	recv(socket_desc, (void*) risp, sizeof(risp), 0);

	//controllo del tipo di risposta del server
	if(strncmp(risp, "OK", 2) == 0){
		//richiesta andata a buon fine
		printf("oggetto preso\n");
		return 0;
	}
	else if(strncmp(risp, "NC", 2) == 0){
		//oggetto non raccoglibile
		printf("non puoi prendere questo oggetto\n");
		return 0;
	}
	else if(strncmp(risp, "TS", 2) == 0){
		//tempo in partita scaduto
		printf("TEMPO SCADUTO\n");
		notifica_player(2);
		return 1;
	}
	else if(strncmp(risp, "NS", 2) == 0){
		//spazio insufficiente
		printf("non hai spazio nell'inventario\n");
		return 0;
	}
	else if(strncmp(risp, "NF", 2) == 0){
		//oggetto non esistente
		printf("oggetto non esistente\n");
		return 0;
	}
	else if(strncmp(risp, "AT", 2) == 0){
		//oggetto già raccolto
		printf("oggetto già presente nell'inventario\n");
		return 0;
	}
	else if(strncmp(risp, "LK", 2) == 0){
		//gestione dell'oggetto bloccato da enigma
		char req[REQ_SIZE];
		char enigma_desc[2 + MAX_DESCRIPTION_LENGTH];
		char risposta[MAX_NAME_LENGTH];

		//richiesta dell'enigma
		memset(req, 0, sizeof(req));
		strcpy(req, "enigma ");
		strcat(req, arg1);
		check(send(socket_desc, req, sizeof(req), 0));
		check(recv(socket_desc, enigma_desc, sizeof(enigma_desc), 0));

		if(strncmp(enigma_desc, "RO", 2) == 0){
			//caso in cui l'oggetto si sblocchi con use
			printf("questo oggetto è bloccato\n");
			return 0;
		}
		else if(strncmp(enigma_desc, "OK", 2) == 0){
			//gestione dell'enigma di tipo indovinello
			printf("oggetto bloccato da enigma:\n");
			printf("%s", &enigma_desc[2]);

			//risposta all'enigma
			memset(risposta, 0, sizeof(risposta));
			fgets(risposta, MAX_NAME_LENGTH, stdin);
			sscanf(risposta, "%s", risposta);
			memset(req, 0, sizeof(req));

			//composizione richiesta
			strcpy(req, "unlock ");
			strcat(req, arg1);
			strcat(req, " ");
			strcat(req, risposta);
			check(send(socket_desc, req, sizeof(req), 0));

			//ricezione esito
			check(recv(socket_desc, enigma_desc, sizeof(enigma_desc), 0));
			if(strncmp(enigma_desc, "WI", 2) == 0){
				printf("HAI COMPLETATO IL LIVELLO\nPARTITA TERMINATA\n");
				notifica_player(1);
				return 1;
			}

			//non si controllano gli altri codici perchè sappiamo già che l'oggetto esiste
			//ed è bloccato da enigma, perciò in ogni caso la risposta del server conterrà
			//un messaggio da mostrare a video
			printf("%s", &enigma_desc[2]);
			return 0;
		}
		else{
			printf("errore\n");
		}
	}
	return 0;
}

// ---------- USE ----------
//comunicazione al server dell'utilizzo di uno o più oggetti
int use_cmd(int narg, char* arg1, char* arg2){
	char req[REQ_SIZE];
	char risp[2 + MAX_DESCRIPTION_LENGTH];

	//controllo formato
	if(narg <= 0){
		printf("CORRETTO UTILIZZO DI USE: use <object1> [object2]\n");
		return 0;
	}

	//composizione comando
	memset(req, 0, sizeof(req));
	memset(risp, 0, sizeof(risp));
	strcpy(req, "use ");
	strcat(req, arg1);
	if(narg > 1){
		strcat(req, " ");
		strcat(req, arg2);
	}

	check(send(socket_desc, req, sizeof(req), 0));	
	check(recv(socket_desc, risp, sizeof(risp), 0));

	//tempo scaduto
	if(strncmp(risp, "TS", 2) == 0){
		printf("TEMPO SCADUTO\n");
		notifica_player(2);
		return 1;
	}

	//oggetto non trovato
	if(strncmp(risp, "NF", 2) == 0){
		printf("oggetto/i non esistente/i\n");
		return 0;
	}

	//oggetto non nell'inventario
	if(strncmp(risp, "NT", 2) == 0){
		printf("%s non presente nell'inventario\n", arg1);
		return 0;
	}

	//oggetto non bloccato da enigma
	if(strncmp(risp, "NL", 2) == 0){
		if(narg == 1){
			printf("%s non richiede l'utilizzo di un oggetto\n", arg1);
		}
		else{
			printf("%s non richiede l'utilizzo di un oggetto\n", arg2);
		}
		return 0;
	}

	if(strncmp(risp, "NU", 2) == 0){
		//l'oggetto utilizzato è sbagliato
		printf("sbagliato: %s", &risp[2]);
	}
	else if(strncmp(risp, "OK", 2) == 0){
		//oggetto utilizzato giusto
		printf("corretto: %s", &risp[2]);
	}
	else if(strncmp(risp, "WI", 2) == 0){
		printf("HAI COMPLETATO IL LIVELLO\nPARTITA TERMINATA\n");
		notifica_player(1);
		return 1;
	}
	return 0;
}

// ---------- OBJS ----------
//richiesta degli oggetti nell'inventario
int objs_cmd(){
	char req[REQ_SIZE];
	char risp[2 + MAX_DESCRIPTION_LENGTH];

	memset(req, 0, sizeof(req));
	memset(risp, 0, sizeof(req));

	//composizione richiesta, invio e ricezione risposta
	strcpy(req, "objs");
	check(send(socket_desc, req, sizeof(req), 0));
	check(recv(socket_desc, risp, sizeof(risp), 0));

	if(strncmp(risp, "NI", 2) == 0){
		printf("inventario vuoto\n");
		return 0;
	}
	else if(strncmp(risp, "TS", 2) == 0){
		printf("TEMPO SCADUTO\n");
		notifica_player(2);
		return 1;
	}
	printf("%s", &risp[2]);
	return 0;
}

// ---------- END ----------
//comunicazione al server della volontà di concludere la partita e chiudere la connessione
void end_cmd(){
	char req[REQ_SIZE];

	//composizione richiesta
	memset(req, 0, sizeof(req));
	strcpy(req, "end");
	check(send(socket_desc, req, sizeof(req), 0));

	printf("PARTITA TERMINATA\n");
	notifica_player(0);

	//terminazione connessione
	logout();
}

// ---------- DEPOSIT ----------
//deposito di un oggetto specificato
int deposit_cmd(int narg, char* arg1){
	char req[REQ_SIZE];
	char risp[2];

	if(narg != 1){
		printf("CORRETTO UTILIZZO DI DEP: dep <object>\n");
		return 0;
	}

	//composizione richiesta
	memset(req, 0, sizeof(req));
	memset(risp, 0, sizeof(risp));
	strcpy(req, "dep ");
	strcat(req, arg1);

	check(send(socket_desc, req, sizeof(req), 0));
	check(recv(socket_desc, risp, sizeof(risp), 0));

	if(strncmp(risp, "TS", 2) == 0){
		printf("TEMPO SCADUTO\n");
		notifica_player(2);
		return 1;
	}

	if(strncmp(risp, "NT", 2) == 0){
		printf("oggetto non raccolto\n");
		return 0;
	}

	if(strncmp(risp, "OK", 2) == 0){
		printf("oggetto depositato nella corrispettiva location\n");
		return 0;
	}
	printf("oggetto non presente nell'inventario\n");
	return 0;
}

// ------------------------------------------------------------
// |          PARTE DEDICATA ALLA FUNZIONE A PIACERE          |
// ------------------------------------------------------------

//inizializza il listener su cui il giocatore 1 attenderà il giocatore 2
void inizializzazione_messaggistica(){
	int res, i;
	struct sockaddr_in addr;
	
	//creazione listener
	listener = socket(AF_INET, SOCK_STREAM, 0);
	check(listener);
	FD_SET(listener, &master);
	if(fd_max < listener)
		fd_max = listener;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(msg_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	res = bind(listener, (struct sockaddr*) &addr, sizeof(addr));
	check(res);
	res = listen(listener, 10);
	check(res);
	FD_SET(listener, &master);
	if(fd_max < listener)
		fd_max = listener;
}

//richiede la porta e si connette al giocatore 1
void connessione_messaggistica(){
	char req[REQ_SIZE];
	char risp[2 + MAX_NAME_LENGTH];

	memset(req, 0, sizeof(req));
	memset(risp, 0, sizeof(risp));

	strcpy(req, "port");
	check(send(socket_desc, req, sizeof(req), 0));
	check(recv(socket_desc, risp, sizeof(risp), 0));

	if(strncmp(risp, "OK", 2) == 0){
		msg_desc = crea_connessione(atoi(&risp[2]));
		check(msg_desc);
		FD_SET(msg_desc, &master);
		if(fd_max < msg_desc)
			fd_max = msg_desc;
	}
}

//manda il messaggio inserito dall'utente all'altro giocatore
void msg_player(){
	char msg[1 + MAX_DESCRIPTION_LENGTH];

	if(msg_desc < 0){
		printf("chat non inizializzata\n");
		return;
	}

	memset(msg, 0, sizeof(msg));
	msg[0] = 'M';
	fgets(&msg[1], MAX_DESCRIPTION_LENGTH, stdin);
	check(send(msg_desc, msg, sizeof(msg), 0));
}

//controlla il tipo di messaggio ricevuto dall'altro giocatore
int gestione_messaggi(){
	char msg[1 + MAX_DESCRIPTION_LENGTH];

	memset(msg, 0, sizeof(msg));

	check(recv(msg_desc, msg, sizeof(msg), 0));
	if(msg[0] == 'M'){
		//il messaggio è di tipo chat
		if(nplayer == 1)
			printf("[GIOCATORE 2]: ");
		else
			printf("[GIOCATORE 1]: ");
		printf("%s\n\n", &msg[1]);
		return 0;
	}

	if(msg[0] == 'E'){
		//il messaggio è di terminazione partita
		printf("PARTITA CONCLUSA DALL'ALTRO GIOCATORE\n");
		return 1;
	}

	if(msg[0] == 'W'){
		//il messaggio è di tipo vittoria partita
		printf("PARTITA VINTA\n");
		return 1;
	}

	if(msg[0] == 'T'){
		//il messaggio è di tipo tempo scaduto in partita
		printf("TEMPO SCADUTO\n");
		return 1;
	}
	return 0;
}

//comunica all'altro giocatore lo stato della partita
//0 partita terminata, 1 partita vinta, 2 tempo scaduto
void notifica_player(int tipo){
	char msg[1 + MAX_DESCRIPTION_LENGTH];

	if(msg_desc < 0)
		return;

	if(tipo == 0)
		msg[0] = 'E';
	else if(tipo == 1)
		msg[0] = 'W';
	else if(tipo == 2)
		msg[0] = 'T';
	else
		return;

	check(send(msg_desc, msg, sizeof(msg), 0));
	close(msg_desc);
	msg_desc = -1;
}
