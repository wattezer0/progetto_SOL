#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "queue.h"
#ifdef __WIN32__
# include <winsock2.h>
#else
# include <sys/socket.h>
#endif

#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "./mysock"

struct w_command {
  int w_flag;
  char * dirname;
  int n;
};

struct D_command {
  int d_flag;
  char * dirname;
};

struct f_command {
  int f_flag;
  char * filename;
};

typedef struct flags {
  int h_flag;  //-h :stampa la lista di tutte le opzioni accettate dal serv_addr e termina immediatamente;
  struct f_command f;  //-f filename : specifica il nome del socket AF_UNIX a cui connettersi;
  struct w_command w;  //-w dirname[,n=0] : invia al server i file nella cartella ‘dirname’, ovvero effettua una richiesta di scrittura al server per i file. Se la directory ‘dirname’ contiene altre directory, queste vengono visitate ricorsivamente fino a quando non si leggono ‘n‘ file; se n=0 (o non è specificato) non c’è un limite superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti).
  int W_flag;  //-W file1[,file2]: lista di nomi di file da scrivere nel server separati da ‘,’;
  struct D_command D;  //-D dirname : cartella in memoria secondaria dove vengono scritti (lato serv_addr) i file che il server rimuove a seguito di capacity misses (e che erano stati modificati da operazioni di scrittura) per servire le scritture richieste attraverso l’opzione ‘-w’ e ‘-W’. L’opzione ‘-D’ deve essere usata quindi congiuntamente all’opzione ‘-w’ o ‘-W’, altrimenti viene generato un errore. Se l’opzione ‘-D’ non viene specificata, tutti i file che il server invia verso il serv_addr a seguito di espulsioni dalla cache, vengono buttati via. Ad esempio, supponiamo i seguenti argomenti a linea di comando “-w send -D store”, e supponiamo che dentro la cartella ‘send’ ci sia solo il file ‘pippo’. Infine supponiamo che il server, per poter scrivere nello storage il file ‘pippo’ deve espellere il file ‘pluto’ e ‘minni’. Allora, al termine dell’operazione di scrittura, la cartella ‘store’ conterrà sia il file ‘pluto’ che il file ‘minni’. Se l’opzione ‘-D’ non viene specificata, allora il server invia sempre i file ‘pluto’ e ‘minni’ al serv_addr, ma questi vengono buttati via;
  int r_flag;  //-r file1[,file2] : lista di nomi di file da leggere dal server separati da ‘,’ (esempio: -r pippo,pluto,minni);
  int R_flag;  //-R [n=0] : tale opzione permette di leggere ‘n’ file qualsiasi attualmente memorizzati nel server; se n=0 (o non è specificato) allora vengono letti tutti i file presenti nel server;
  int d_flag;  //-d dirname : cartella in memoria secondaria dove scrivere i file letti dal server con l’opzione ‘-r’ o ‘-R’. L’opzione -d va usata congiuntamente a ‘-r’ o ‘-R’, altrimenti viene generato un errore; Se si utilizzano le opzioni ‘-r’ o ‘-R’ senza specificare l’opzione ‘-d’ i file letti non vengono memorizzati sul disco;
  int t_flag;  //-t time : tempo in millisecondi che intercorre tra l’invio di due richieste successive al server (se non specificata si suppone -t 0, cioè non c’è alcun ritardo tra l’invio di due richieste consecutive);
  int l_flag;  //-l file1[,file2] : lista di nomi di file su cui acquisire la mutua esclusione;
  int u_flag;  //-u file1[,file2] : lista di nomi di file su cui rilasciare la mutua esclusione;
  int c_flag;  //-c file1[,file2] : lista di file da rimuovere dal server se presenti;
  int p_flag;  //-p : abilita le stampe sullo standard output per ogni operazione. Le stampe associate alle varie operazioni riportano almeno le seguenti informazioni: tipo di operazione, file di riferimento, esito e dove è rilevante i bytes letti o scritti.
} serv_addr_flags;

serv_addr_flags serv_addr;

void inizialize_flags (serv_addr_flags * c) {
  c->h_flag = c->f.f_flag = c->w.w_flag = c->W_flag =  c->D.d_flag = c->R_flag =  c->r_flag =  c->d_flag = c->t_flag =  c->l_flag = c->u_flag = c->c_flag = c->p_flag = 0;
}

//dato in input il fd_c risalgo alle richieste del serv_addr
void find_requests (int fd_c, int argc, char ** argv) {
  
  //estraggo la struct serv_addr_flags da fd_c
  
  int c;
  while ((c = getopt (argc, argv, "hfw:WD:rRdtlucp:")) != -1)
    switch (c)
      {
      case 'h':
        serv_addr.h_flag = 1;
        break;
      case 'f':
        serv_addr.f.f_flag = 1;
        serv_addr.f.filename = (char *) malloc (100*sizeof(char));
        printf("Inserisci il nome del socket a cui connettersi:\n");
        scanf("%s",serv_addr.f.filename);
        break;
      case 'w':
        serv_addr.w.w_flag = 1;
        serv_addr.w.dirname = (char *) malloc (100*sizeof(char));
        printf("Inserisci il nome della cartella da inviare al server:\n");
        scanf("%s",serv_addr.w.dirname);
        printf("Inserisci il numero di file da considerare:\n");
        scanf("%d", &serv_addr.w.n);
        break;
      case 'W':
        serv_addr.W_flag = 1;
        break;
      case 'D':
        serv_addr.D.d_flag= 1;
        serv_addr.D.dirname = (char *) malloc (100*sizeof(char));
        //printf("Inserisci il nome della cartella dove fare lo store dei file eliminati:\n");
        //scanf("%s",serv_addr.D.dirname);
        serv_addr.D.dirname = optarg;
        break;
      case 'r':
        serv_addr.r_flag = 1;
        break;
      case 'R':
        serv_addr.R_flag = 1;
        break;
      case 'd':
        serv_addr.d_flag = 1;
        break;
      case 't':
        serv_addr.t_flag = 1;
        break;
      case 'l':
        serv_addr.l_flag = 1;
        break;
      case 'u':
        serv_addr.u_flag = 1;
        break;
      case 'c':
        serv_addr.c_flag = 1;
        break;
      case 'p':
        serv_addr.p_flag = 1;
        break;
      case '?':
        if (optopt == 'D')
          fprintf (stderr, "L'opzione -%c richiede un argomento\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Opzione `-%c' non riconosciuta.\n", optopt);
        else
          fprintf (stderr, "Carattere`\\x%x'. non riconosciuto\n", optopt);
      default: abort ();
      }

}

int main (int argc, char **argv)
{
  //estrazione delle flags
  int index;
  inizialize_flags(&serv_addr);
  find_requests(1,argc,argv);
  printf ("**FLAG DEL serv_addr**\nh flag = %d\nf flag = %d , filename = %s\nw flag = %d, dirname = %s , n = %d\nW flag = %d\nD flag = %d, dirname = %s\nr flag = %d\nR flag = %d\nd flag = %d\nt flag = %d\nl flag = %d\nu flag = %d\nc flag = %d\np flag = %d\n", serv_addr.h_flag, serv_addr.f.f_flag, serv_addr.f.filename, serv_addr.w.w_flag,serv_addr.w.dirname,serv_addr.w.n, serv_addr.W_flag, serv_addr.D.d_flag,serv_addr.D.dirname,serv_addr.r_flag, serv_addr.R_flag, serv_addr.d_flag, serv_addr.t_flag, serv_addr.l_flag, serv_addr.u_flag, serv_addr.c_flag, serv_addr.p_flag);
  for (index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);

  //connessione al server
  int fd_skt, fd_c;
  fd_skt=socket(AF_UNIX,SOCK_STREAM,0);
      memset(&serv_addr, '0', sizeof(serv_addr));
  struct sockaddr_un serv_addr;
  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path, SOCKNAME, UNIX_PATH_MAX);
  printf("%d\n", errno);
  //parte di esempio
  char buf[100];
  if (connect(fd_skt,(struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1 ) {
    printf("Non Sono connesso!\n");
     
    if ( errno == ENOENT ) 
      sleep(1); /* sock non esiste */
    else exit(EXIT_FAILURE); }
  write(fd_skt,"Hallo!",7);
  read(fd_skt,buf,100);
  printf("serv_addr got: %s\n",buf) ;
  close(fd_skt); 
  
  return 0;
}