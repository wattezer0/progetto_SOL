sleep 5

cd Server
./server_project config2.txt &

cd ../Client
CWD=$(pwd)

echo "Running client -t 200 -p -w $CWD/prova"
./client_project -t 200 -p -w "$CWD/prova"
./client_project -t 200 -p -w "$CWD/prova"
./client_project -t 200 -p -w "$CWD/prova"


kill -s HUP $(ps aux | grep server | grep -v grep | awk -F " " '{print $2}')
