all:
	g++ -c -fPIC historydb-fastcgi.cpp
	g++ -shared historydb-fastcgi.o -lfastcgi-daemon2 -pthread -o historydb-fastcgi.so
	fastcgi-daemon2 --config=fastcgi.conf