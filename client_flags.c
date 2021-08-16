#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/types.h>
#include "queue.h"
#ifdef __WIN32__
# include <winsock2.h>
#else
# include <sys/socket.h>
#endif

#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "./mysock"

typedef struct flags {
  int h_flag;  //-h :stampa la lista di tutte le opzioni accettate dal client e termina immediatamente;
  int f_flag;  //-f filename : specifica il nome del socket AF_UNIX a cui connettersi;
  int w_flag;  //-w dirname[,n=0] : invia al server i file nella cartella ‘dirname’, ovvero effettua una richiesta di scrittura al server per i file. Se la directory ‘dirname’ contiene altre directory, queste vengono visitate ricorsivamente fino a quando non si leggono ‘n‘ file; se n=0 (o non è specificato) non c’è un limite superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti).
  int W_flag;  //-W file1[,file2]: lista di nomi di file da scrivere nel server separati da ‘,’;
  int D_flag;  //-D dirname : cartella in memoria secondaria dove vengono scritti (lato client) i file che il server rimuove a seguito di capacity misses (e che erano stati modificati da operazioni di scrittura) per servire le scritture richieste attraverso l’opzione ‘-w’ e ‘-W’. L’opzione ‘-D’ deve essere usata quindi congiuntamente all’opzione ‘-w’ o ‘-W’, altrimenti viene generato un errore. Se l’opzione ‘-D’ non viene specificata, tutti i file che il server invia verso il client a seguito di espulsioni dalla cache, vengono buttati via. Ad esempio, supponiamo i seguenti argomenti a linea di comando “-w send -D store”, e supponiamo che dentro la cartella ‘send’ ci sia solo il file ‘pippo’. Infine supponiamo che il server, per poter scrivere nello storage il file ‘pippo’ deve espellere il file ‘pluto’ e ‘minni’. Allora, al termine dell’operazione di scrittura, la cartella ‘store’ conterrà sia il file ‘pluto’ che il file ‘minni’. Se l’opzione ‘-D’ non viene specificata, allora il server invia sempre i file ‘pluto’ e ‘minni’ al client, ma questi vengono buttati via;
  int r_flag;  //-r file1[,file2] : lista di nomi di file da leggere dal server separati da ‘,’ (esempio: -r pippo,pluto,minni);
  int R_flag;  //-R [n=0] : tale opzione permette di leggere ‘n’ file qualsiasi attualmente memorizzati nel server; se n=0 (o non è specificato) allora vengono letti tutti i file presenti nel server;
  int d_flag;  //-d dirname : cartella in memoria secondaria dove scrivere i file letti dal server con l’opzione ‘-r’ o ‘-R’. L’opzione -d va usata congiuntamente a ‘-r’ o ‘-R’, altrimenti viene generato un errore; Se si utilizzano le opzioni ‘-r’ o ‘-R’ senza specificare l’opzione ‘-d’ i file letti non vengono memorizzati sul disco;
  int t_flag;  //-t time : tempo in millisecondi che intercorre tra l’invio di due richieste successive al server (se non specificata si suppone -t 0, cioè non c’è alcun ritardo tra l’invio di due richieste consecutive);
  int l_flag;  //-l file1[,file2] : lista di nomi di file su cui acquisire la mutua esclusione;
  int u_flag;  //-u file1[,file2] : lista di nomi di file su cui rilasciare la mutua esclusione;
  int c_flag;  //-c file1[,file2] : lista di file da rimuovere dal server se presenti;
  int p_flag;  //-p : abilita le stampe sullo standard output per ogni operazione. Le stampe associate alle varie operazioni riportano almeno le seguenti informazioni: tipo di operazione, file di riferimento, esito e dove è rilevante i bytes letti o scritti.
} client_flags;

client_flags client;

void inizialize_flags (client_flags * c) {
  c->h_flag = c->f_flag = c->w_flag = c->W_flag =  c->D_flag = c->R_flag =  c->r_flag =  c->d_flag = c->t_flag =  c->l_flag = c->u_flag = c->c_flag = c->p_flag = 0;
}

//dato in input il fd_c risalgo alle richieste del client
void find_requests (int fd_c, int argc, char ** argv) {
  
  //estraggo la struct client_flags da fd_c
  
  int c;
  while ((c = getopt (argc, argv, "hfwWDrRdtlucp:")) != -1)
    switch (c)
      {
      case 'h':
        client.h_flag = 1;
        break;
      case 'f':
        client.f_flag = 1;
        break;
      case 'w':
        client.w_flag = 1;
        break;
      case 'W':
        client.W_flag = 1;
        break;
      case 'D':
        client.D_flag = 1;
        break;
      case 'r':
        client.r_flag = 1;
        break;
      case 'R':
        client.R_flag = 1;
        break;
      case 'd':
        client.d_flag = 1;
        break;
      case 't':
        client.t_flag = 1;
        break;
      case 'l':
        client.l_flag = 1;
        break;
      case 'u':
        client.u_flag = 1;
        break;
      case 'c':
        client.c_flag = 1;
        break;
      case 'p':
        client.p_flag = 1;
        break;
      case '?':
        if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
      default: abort ();
      }

}

int main (int argc, char **argv)
{
  //estrazione delle flags
  int index;
  opterr = 0;
  inizialize_flags(&client);
  find_requests(1,argc,argv);
  printf ("**FLAG DEL CLIENT**\nh flag = %d\nf flag = %d\nw flag = %d\nW flag = %d\nD flag = %d\nr flag = %d\nR flag = %d\nd flag = %d\nt flag = %d\nl flag = %d\nu flag = %d\nc flag = %d\np flag = %d\n", client.h_flag, client.f_flag, client.w_flag, client.W_flag, client.D_flag,client.r_flag, client.R_flag, client.d_flag, client.t_flag, client.l_flag, client.u_flag, client.c_flag, client.p_flag);
  for (index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);

  //connessione al server
  int fd_skt, fd_c;
  fd_skt=socket(AF_UNIX,SOCK_STREAM,0);
  struct sockaddr client;
  client.sa_family = AF_UNIX;
  strncpy(client.sa_data, SOCKNAME, UNIX_PATH_MAX);

  //parte di esempio
  char buf[100];
  while (connect(fd_skt,(struct sockaddr*)&client, sizeof(client)) == -1 ) {
    printf("-1\n");
    if ( errno == ENOENT ) 
      sleep(1); /* sock non esiste */
    else exit(EXIT_FAILURE); }
  write(fd_skt,"Hallo!",7);
  read(fd_skt,buf,100);
  printf("Client got: %s\n",buf) ;
  close(fd_skt);   

  return 0;
}