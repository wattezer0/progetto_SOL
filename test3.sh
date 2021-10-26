#!/bin/bash
cd Server
#./server_project config3.txt &
cd ../Client
CWD=$(pwd)

START=`date +%s`

while [ $(( $(date +%s) - 30)) -lt $START ]; do #loop per 30 secondi

	while [ $(ps -ef | grep -v grep | grep client_project | wc -l) -lt 10 ]; do #conta 
		./client_project -t 0 -w "$CWD/prova" -D "$CWD/dest"  -W "$CWD/prova/file5" -u "$CWD/prova/file5" &
		
		
		
		./client_project -t 0 -W "$CWD/prova/file3" "$CWD/prova/prova1/file10" -D "$CWD/dest" -r "$CWD/prova/prova1/file10" -d "$CWD/destR" &
		
		
		./client_project -t 0 -r "$CWD/prova/prova1/file11" -d "$CWD/destR" -u "$CWD/prova/prova1/file11" &
		
		
		
		
		./client_project -t 0 -W "$CWD/prova/file3" "$CWD/prova/file5" -r "$CWD/prova/prova3/file7" -d "$CWD/destR" -D "$CWD/dest" &
		printf  "\rNUMERO DI CLIENT IN ESECUZIONE : $(ps -ef | grep -v grep | grep client_project | wc -l)"
	done
done
echo "while terminato-------------"

kill -s INT $(ps aux | grep server_project | grep -v grep | awk -F " " '{print $2}')
while [ $(( $(date +%s) - 5)) -lt $START ]; do
	printf  "\rNUMERO DI CLIENT IN ESECUZIONE : $(ps -ef | grep -v grep | grep client_project | wc -l)"

done
cd ..

