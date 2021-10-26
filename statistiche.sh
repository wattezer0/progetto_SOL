#!/bin/bash
cd Server
echo "*****STATISTICHE*****"
echo "* NUMERO DI OPERAZIONI DI OPENFILE : $(grep -c "RICHIESTA DI APERTURA" Log_file.txt)"


echo "* NUMERO DI OPERAZIONI DI READFILE : $(grep -c "RICHIESTA DI LETTURA" Log_file.txt)"

grep "BYTE LETTI" Log_file.txt | sort -k 3n | awk -F 'BYTE LETTI: ' '{print $1, $2}' | awk '{s+=$1}END{print "* MEDIA DELLE LETTURE IN BYTE : ",s/(NR -1)}' RS=" " 

echo "* NUMERO DI OPERAZIONI DI WRITEFILE : $(grep -c "RICHIESTA DI SCRITTURA" Log_file.txt)"


cont=$(grep -c "BYTE SCRITTI" Log_file.txt)
grep "BYTE SCRITTI : " Log_file.txt | sort -k 3n | awk -F 'BYTE SCRITTI : ' '{print $1, $2}' | awk '{s+=$1}END{print "* MEDIA DELLE SCRITTURE IN BYTE : ",s/(NR -1)}' RS=" " 

echo "* NUMERO DI OPERAZIONI DI APPENDTOFILE : $(grep -c "RICHIESTA DI APPEND" Log_file.txt)"

echo "* NUMERO DI OPERAZIONI DI LOCKFILE : $(grep -c "RICHIESTA DI LOCK" Log_file.txt)"

echo "* NUMERO DI OPERAZIONI DI UNLOCKFILE : $(grep -c "RICHIESTA DI UNLOCK" Log_file.txt)"

echo "* NUMERO DI OPERAZIONI DI CLOSEFILE : $(grep -c "RICHIESTA DI CHIUSURA FILE" Log_file.txt)"

echo "* NUMERO DI OPERAZIONI DI CLOSECONNECTION : $(grep -c "RICHIESTA DI CHIUSURA CONNESSIONE" Log_file.txt)"

echo "* NUMERO DI RICHIESTE PER OGNI THREAD WORKER"
grep "THREAD WORKER IN ESECUZIONE" Log_file.txt | awk -F 'THREAD WORKER IN ESECUZIONE ' '{print " RICHIESTE per il THREAD WORKER "$1, $2}' | sort | uniq -c | head -n8


grep "NUMERO DI CONNESSIONI ATTUALI :" Log_file.txt | sort -k 6n | tail -n1 | awk -F 'NUMERO DI CONNESSIONI ATTUALI : ' '{print "* MASSIMO NUMERO DI CONNESSIONI CONTEMPORANEE : " $1, $2}'

echo "* NUMERO MASSIMO DI FILE MEMORIZZATI NEL SERVER : $(grep "Numero di file massimo memorizzato nel server" Log_file.txt | awk -F '	-Numero di file massimo memorizzato nel server =' '{print $1,$2}')"

grep "Numero di volte in cui l'algoritmo di rimpiazzamento della cache è stato eseguito per selezionare uno o più file vittima" Log_file.txt | awk -F '	-' '{print "*"$1, $2}'

grep "Dimensione massima in Bytes raggiunta dal file storage" Log_file.txt | awk -F '	-' '{print "*"$1,$2}'


