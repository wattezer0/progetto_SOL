sleep 5

cd Server
valgrind --leak-check=full ./server_project config1.txt &

cd ../Client
CWD=$(pwd)

echo "Running client -t 200 -p -W $CWD/prova/file1"
./client_project -t 200 -p -W "$CWD/prova/file1"
sleep .2

echo "Running client -t 200 -p -w $CWD/prova"
./client_project -t 200 -p -w "$CWD/prova"
sleep .2

echo "Running client -t 200 -p -r $CWD/prova/file1 -d $CWD/destR"
./client_project -t 200 -p  -r "$CWD/prova/file1" -d "$CWD/destR"
sleep .2

echo "Running client -t 200 -p -w $CWD/prova -d $CWD/destR -R 0"
./client_project -t 200 -p -w "$CWD/prova" -d "$CWD/destR" -R 0


kill -s HUP $(ps aux | grep server_project | grep -v grep | awk -F " " '{print $2}')

cd ..

