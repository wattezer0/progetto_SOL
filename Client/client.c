/* We want POSIX.1-2008 + XSI, i.e. SuSv4, features */
#define _XOPEN_SOURCE 700

/* Added on 2017-06-25:
   If the C library can support 64-bit file sizes
   and offsets, using the standard names,
   these defines tell the C library to do so. */
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64 

#include "treewalk.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ftw.h>

#define SOCKNAME "../Server/socket.skt"


struct w_command {
  int flag;
  char dirname[256];
  int n;
};

struct command {
  int flag;
  char name[256];
};

struct command_int {
  int flag;
  int k;
};

struct command_list {
  int flag;
  char list[256][256];
  int dim;
};

typedef struct flags {
  int h;  //-h :stampa la lista di tutte le opzioni accettate dal serv_addr e termina immediatamente;
  struct command f;  //-f filename : specifica il nome del socket AF_UNIX a cui connettersi;
  struct w_command w;  //-w dirname[,n=0] : invia al server i file nella cartella ‘dirname’, ovvero effettua una richiesta di scrittura al server per i file. Se la directory ‘dirname’ contiene altre directory, queste vengono visitate ricorsivamente fino a quando non si leggono ‘n‘ file; se n=0 (o non è specificato) non c’è un limite superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti).
  struct command_list W;  //-W file1[,file2]: lista di nomi di file da scrivere nel server;
  struct command D;  //-D dirname : cartella in memoria secondaria dove vengono scritti (lato serv_addr) i file che il server rimuove a seguito di capacity misses (e che erano stati modificati da operazioni di scrittura) per servire le scritture richieste attraverso l’opzione ‘-w’ e ‘-W’. L’opzione ‘-D’ deve essere usata quindi congiuntamente all’opzione ‘-w’ o ‘-W’, altrimenti viene generato un errore. Se l’opzione ‘-D’ non viene specificata, tutti i file che il server invia verso il serv_addr a seguito di espulsioni dalla cache, vengono buttati via. Ad esempio, supponiamo i seguenti argomenti a linea di comando “-w send -D store”, e supponiamo che dentro la cartella ‘send’ ci sia solo il file ‘pippo’. Infine supponiamo che il server, per poter scrivere nello storage il file ‘pippo’ deve espellere il file ‘pluto’ e ‘minni’. Allora, al termine dell’operazione di scrittura, la cartella ‘store’ conterrà sia il file ‘pluto’ che il file ‘minni’. Se l’opzione ‘-D’ non viene specificata, allora il server invia sempre i file ‘pluto’ e ‘minni’ al serv_addr, ma questi vengono buttati via;
  struct command_list r;  //-r file1[,file2] : lista di nomi di file da leggere dal server separati da ‘,’ (esempio: -r pippo,pluto,minni);
  struct command_int R;  //-R [n=0] : tale opzione permette di leggere ‘n’ file qualsiasi attualmente memorizzati nel server; se n=0 (o non è specificato) allora vengono letti tutti i file presenti nel server;
  struct command d;  //-d dirname : cartella in memoria secondaria dove scrivere i file letti dal server con l’opzione ‘-r’ o ‘-R’. L’opzione -d va usata congiuntamente a ‘-r’ o ‘-R’, altrimenti viene generato un errore; Se si utilizzano le opzioni ‘-r’ o ‘-R’ senza specificare l’opzione ‘-d’ i file letti non vengono memorizzati sul disco;
  struct command_int t;  //-t time : tempo in millisecondi che intercorre tra l’invio di due richieste successive al server (se non specificata si suppone -t 0, cioè non c’è alcun ritardo tra l’invio di due richieste consecutive);
  struct command_list l;  //-l file1[,file2] : lista di nomi di file su cui acquisire la mutua esclusione;
  struct command_list u;  //-u file1[,file2] : lista di nomi di file su cui rilasciare la mutua esclusione;
  struct command_list c;  //-c file1[,file2] : lista di file da rimuovere dal server se presenti;
  int p;  //-p : abilita le stampe sullo standard output per ogni operazione. Le stampe associate alle varie operazioni riportano almeno le seguenti informazioni: tipo di operazione, file di riferimento, esito e dove è rilevante i bytes letti o scritti.
} client_addr_flags;

client_addr_flags client_flags;

//INIZIALIZZAZIONE DELLE FLAG
void inizialize_flags (client_addr_flags * c) {
  c-> h = c->f.flag = c->w.flag = c->W.flag =  c->D.flag = c->R.flag =  c->r.flag =  c->d.flag = c->t.flag =  c->l.flag = c->u.flag = c->c.flag = c->p = 0;
  c->w.n = c->R.k = c->t.k = 0;
  c->W.dim = c->r.dim = c->l.dim = c->u.dim = c->c.dim = 0;
}

//ASSEGNAMENTO DELLE RICHIESTE 
void find_requests (int fd_c, int argc, char ** argv) {
  
  //estraggo la struct client_addr_flags da fd_c
  
  int c;
  while ((c = getopt (argc, argv, "hf:w:W:D:r:R::d:t::l:u:c:p")) != -1)
    switch (c)
      {
      case 'h':
        client_flags.h = 1;
        break;
      case 'f':
        client_flags.f.flag = 1;
        strcpy(client_flags.f.name, optarg);
        break;
      case 'w':
        if (strchr(argv[optind -1], (int) '-') == NULL) {
          client_flags.w.flag = 1;
          strcpy(client_flags.w.dirname, argv[optind -1]);
          if ( optind < argc && strchr(argv[optind], (int) '-') == NULL )
            client_flags.w.n = atoi(argv[optind]);
        }
        else {
          fprintf (stderr, "L'opzione -w richiede un dirname\n");
          abort ();
        }
        break;
      case 'W':
      {
        client_flags.W.flag = 1;
        int i = optind -1;
        while (i < argc && strchr(argv[i], (int) '-') == NULL) {
            strcpy(client_flags.W.list[i + 1 - optind], argv[i]);
            i++;
            client_flags.W.dim ++;
        }}
        break;
      case 'D':
        client_flags.D.flag = 1;
        strcpy(client_flags.D.name, optarg);
        break;
      case 'r':
      {
        client_flags.r.flag = 1;
        int i = optind -1;
        while (i < argc && strchr(argv[i], (int) '-') == NULL) {
            strcpy(client_flags.r.list[i + 1 - optind], argv[i]);
            i++;
            client_flags.r.dim ++;
        }}
        break;
      case 'R':
        client_flags.R.flag = 1;
        if ( optind < argc && strchr(argv[optind], (int) '-') == NULL ){
          client_flags.R.k = atoi(argv[optind]);}
        break;
      case 'd':
        client_flags.d.flag = 1;
        strcpy(client_flags.d.name, optarg);
        break;
      case 't':
        client_flags.t.flag = 1;
        if ( optind < argc && strchr(argv[optind], (int) '-') == NULL ){
          client_flags.t.k = atoi(argv[optind]);}
        break;
      case 'l':
       {
        client_flags.l.flag = 1;
        int i = optind -1;
        while (i < argc && strchr(argv[i], (int) '-') == NULL) {
            strcpy(client_flags.l.list[i + 1 - optind], argv[i]);
            i++;
            client_flags.l.dim ++;
        }}
        break;
      case 'u':
        {
        client_flags.u.flag = 1;
        int i = optind -1;
        while (i < argc && strchr(argv[i], (int) '-') == NULL) {
            strcpy(client_flags.u.list[i + 1 - optind] ,  argv[i]);
            i++;
            client_flags.u.dim ++;
        }}
        break;
      case 'c':
        {
        client_flags.c.flag = 1;
        int i = optind -1;
        while (i < argc && strchr(argv[i], (int) '-') == NULL) {
            strcpy(client_flags.c.list[i + 1 - optind] , argv[i]);
            i++;
            client_flags.c.dim ++;
        }}
        break;
      case 'p':
        client_flags
      .p = 1;
        break;
      case '?':
        if (optopt == 'f')
          fprintf (stderr, "L'opzione -%c richiede un socket_filename\n", optopt);
        else if (optopt == 'w')
          fprintf (stderr, "L'opzione -%c richiede un dirname\n", optopt);
        else if (optopt == 'W')
          fprintf (stderr, "L'opzione -%c richiede almeno un filename\n", optopt);
        else if (optopt == 'D')
          fprintf (stderr, "L'opzione -%c richiede un dirname\n", optopt);
        else if (optopt == 'r')
          fprintf (stderr, "L'opzione -%c richiede almeno un filename\n", optopt);
        else if (optopt == 'd')
          fprintf (stderr, "L'opzione -%c richiede un dirname\n", optopt);

        else if (isprint (optopt))
          fprintf (stderr, "Opzione `-%c' non riconosciuta.\n", optopt);
        else
          fprintf (stderr, "Carattere`\\x%x'. non riconosciuto\n", optopt);
      default: abort ();
      }

}

//CONVERTE IL TEMPO 
static void ms2ts(struct timespec *ts, unsigned long ms)
{
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000;
}

//STAMPA LE RICHIESTE DEL CLIENT
void print_flags (client_addr_flags client_flags, int argc, char ** argv) {
  printf("-h = %d\n", client_flags
.h);
  printf("-f = %d , %s\n", client_flags
.f.flag, client_flags
.f.name);
  printf("-w = %d , %s , n = %d\n", client_flags
.w.flag, client_flags
.w.dirname, client_flags
.w.n);

  printf("-W = %d , ", client_flags
.W.flag);
  int i = 0; 
  while ( i < client_flags
.W.dim ){
    printf("%s ", client_flags
  .W.list[i]); 
    i++;  }
  printf("\n");
  printf("-D = %d , %s\n", client_flags
.D.flag, client_flags
.D.name);
  
  printf("-r = %d , ", client_flags
.r.flag);
  i = 0;
  while ( i < client_flags
.r.dim ){
    printf("%s ", client_flags
  .r.list[i]); 
    i++;  }
  printf("\n");

  printf("-R = %d , n = %d\n", client_flags
.R.flag, client_flags
.R.k);
  printf("-d = %d , %s\n", client_flags
.d.flag, client_flags
.d.name);
  printf("-t = %d , n = %d\n", client_flags
.t.flag, client_flags
.t.k);
  printf("-c = %d , ", client_flags
.c.flag);
  i = 0; 
  while ( i < client_flags
.c.dim ){
    printf("%s ", client_flags
  .c.list[i]); 
    i++;  }
  printf("\n");
  
  printf("-l = %d , ", client_flags
.l.flag);
  i = 0; 
  while ( i < client_flags
.l.dim ){
    printf("%s ", client_flags
  .l.list[i]); 
    i++;  }
  printf("\n");
  
  printf("-u = %d , ", client_flags
.u.flag);
  i = 0; 
  while ( i < client_flags
.u.dim ){
    printf("%s ", client_flags.u.list[i]); 
    i++;  }
  printf("\n-p = %d\n", client_flags.p);

}

//CONTROLLA SE CI SONO RICHIESTE DA ELABORARE
int there_are_flags (client_addr_flags c) {
  return  c.h || c.f.flag || c.w.flag || c.W.flag || c.R.flag ||  c.r.flag || c.t.flag ||  c.l.flag || c.u.flag || c.c.flag;

}


int main (int argc, char **argv)
{
  //estrazione delle flags
  inizialize_flags(&client_flags);
  find_requests(1,argc,argv);
  

  struct timespec ts;
  ms2ts(&ts, 3000);
  int connected = -1;
  int wait = 0; //tempo in ms che intercorre tra due richieste successive al server (settato tramite -t)

  //caso in cui è stata settata -D, -w e -W non sono state settate
  if (client_flags.D.flag && !client_flags.w.flag && !client_flags.W.flag) {
    printf("È stata settata -D. È necessario settare anche -w o -W\n");
    exit(EXIT_FAILURE);
  }
  //caso in cui è stata settata -d, -r e -R non sono state settate
  if (client_flags.d.flag && !client_flags.r.flag && !client_flags.R.flag) {
    printf("È stata settata -d. È necessario settare anche -r o -R\n");
    exit(EXIT_FAILURE);
  }
  
  enable_print = client_flags.p;

  // -h
  if (client_flags.h) { 
    client_flags.h = 0;
    printf("-h :stampa la lista di tutte le opzioni accettate dal serv_addr e termina immediatamente;\n");
    printf("\n-f filename : specifica il nome del socket 'AF_UNIX' a cui connettersi;\n");
    printf("\n-w dirname[,n=0] : invia al server i file nella cartella ‘dirname’, ovvero effettua una richiesta di scrittura al server per i file. Se la directory ‘dirname’ contiene altre directory, queste vengono visitate ricorsivamente fino a quando non si leggono ‘n‘ file; se n=0 (o non è specificato) non c’è un limite superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti)\n");
    printf("\n-W file1[,file2]: lista di nomi di file da scrivere nel server;\n");
    printf("\n-D dirname : cartella in memoria secondaria dove vengono scritti (lato serv_addr) i file che il server rimuove a seguito di capacity misses (e che erano stati modificati da operazioni di scrittura) per servire le scritture richieste attraverso l’opzione ‘-w’ e ‘-W’. L’opzione ‘-D’ deve essere usata quindi congiuntamente all’opzione ‘-w’ o ‘-W’, altrimenti viene generato un errore. Se l’opzione ‘-D’ non viene specificata, tutti i file che il server invia verso il serv_addr a seguito di espulsioni dalla cache, vengono buttati via. Ad esempio, supponiamo i seguenti argomenti a linea di comando “-w send -D store”, e supponiamo che dentro la cartella ‘send’ ci sia solo il file ‘pippo’. Infine supponiamo che il server, per poter scrivere nello storage il file ‘pippo’ deve espellere il file ‘pluto’ e ‘minni’. Allora, al termine dell’operazione di scrittura, la cartella ‘store’ conterrà sia il file ‘pluto’ che il file ‘minni’. Se l’opzione ‘-D’ non viene specificata, allora il server invia sempre i file ‘pluto’ e ‘minni’ al serv_addr, ma questi vengono buttati via;\n");
    printf("\n-r file1[,file2] : lista di nomi di file da leggere dal server;\n");
    printf("\n-R [n=0] : tale opzione permette di leggere ‘n’ file qualsiasi attualmente memorizzati nel server; se n=0 (o non è specificato) allora vengono letti tutti i file presenti nel server;\n");
    printf("\n-d dirname : cartella in memoria secondaria dove scrivere i file letti dal server con l’opzione ‘-r’ o ‘-R’. L’opzione -d va usata congiuntamente a ‘-r’ o ‘-R’, altrimenti viene generato un errore; Se si utilizzano le opzioni ‘-r’ o ‘-R’ senza specificare l’opzione ‘-d’ i file letti non vengono memorizzati sul disco;\n");
    printf("\n-t time : tempo in millisecondi che intercorre tra l’invio di due richieste successive al server (se non specificata si suppone -t 0, cioè non c’è alcun ritardo tra l’invio di due richieste consecutive);\n");
    printf("\n-l file1[,file2] : lista di nomi di file su cui acquisire la mutua esclusione;\n");
    printf("\n-u file1[,file2] : lista di nomi di file su cui rilasciare la mutua esclusione;\n");
    printf("\n-c file1[,file2] : lista di file da rimuovere dal server se presenti;\n");
    printf("\n-p : abilita le stampe s1ullo standard output per ogni operazione. Le stampe associate alle varie operazioni riportano almeno le seguenti informazioni: tipo di operazione, file di riferimento, esito e dove è rilevante i bytes letti o scritti.\n");
    return 0;
  }

  /*CICLO CHE ESTRAPOLA LE FLAG E ESEGUE LE RICHIESTE ASSOCIATE*/
  while (there_are_flags(client_flags)) {
    errno = 0;
    
    if (client_flags.t.flag) { //-t time
        client_flags.t.flag = 0;
        wait = client_flags.t.k;
    }

    else if (client_flags.f.flag) { // -f socketname
      client_flags.f.flag = 0;
      char path[100] = "../Server/";
      strcat(path, client_flags.f.name);
      connected = openConnection(path, 1000, ts); //se avviene la connessione connected -> 0 
    }

    else if (connected == -1) { //connessione di default
      connected = openConnection(SOCKNAME, 1000, ts);
    }

    else if (client_flags.w.flag) {
        client_flags.w.flag = 0;
        visit_directory_tree(client_flags.w.dirname, client_flags.w.n, client_flags.D.name);
          if (errno!=0) {
              printf("errno =2= %d\n", errno);
          }
      }

    else if (client_flags.W.flag) {
      client_flags.W.flag = 0;

      for(int i = 0; i < client_flags.W.dim; i++) {
          write_request(client_flags.W.list[i], client_flags.D.name);

          if (errno > 100) {
              return 0;
          }
          else if (errno!=0) {
          }
      }
    }
    else if (client_flags.r.flag) {
      client_flags.r.flag = 0;
      char * buf;
      size_t bufsize = 0;
      for (int i = 0; i < client_flags.r.dim; i++) {
        if (openFile(client_flags.r.list[i], 0) == 0) {
          if (readFile(client_flags.r.list[i], (void **)&buf, &bufsize) == 0) {
            closeFile(client_flags.r.list[i]);
            if (client_flags.d.flag){ //salva il contenuto di buf nella cartella passata come parametro
              FILE * tmpp;  
              char new[256];
              strcpy(new, client_flags.d.name);
              strcat(new, "/");
              removeChar(client_flags.r.list[i], '/');
              strcat(new,client_flags.r.list[i]);
              if((tmpp = fopen(new, "w")) == NULL) {
                printf("Errore nell' apertura del file %s\n", client_flags.r.list[i]);
                return -1;
              }
              fprintf(tmpp, "%s", buf);
              fclose(tmpp);
              free(buf);
            }
            else {
              printf("\nContenuto del file :\n%s\n", buf);
              free(buf);
            }
          }
        }
      }
    }

    else if (client_flags.R.flag) {
      client_flags.R.flag = 0; 
      readNFiles(client_flags.R.k, client_flags.d.name);
    }

    else if (client_flags.l.flag) {
      client_flags.l.flag = 0;
      for (int i = 0; i < client_flags.l.dim; i ++) {
        lockFile(client_flags.l.list[i]);
      }
    }  
    
    else if (client_flags.u.flag) {

      client_flags.u.flag = 0;
      for (int i = 0; i < client_flags.u.dim; i ++) {
        unlockFile(client_flags.u.list[i]);
      }
    }
    else if (client_flags.c.flag) {
      for (int i = 0; i < client_flags.c.dim; i ++) {
        removeFile(client_flags.c.list[i]);
      }
      client_flags.c.flag = 0;
    }


    usleep(wait*1000);
  }


closeConnection(SOCKNAME);

  
return 0;
}