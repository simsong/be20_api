CPPFLAGS=-I.. -I../.. -I. -I/usr/local/include -I/opt/local/include -L/usr/local/lib  -L/opt/local/lib
all:
	(cd ..; make)


stand: feature_sql
feature_sql: feature_recorder_sql.cpp beregex.cpp
	g++ -o stand feature_recorder_sql.cpp beregex.cpp -DSTAND -lsqlite3 $(CPPFLAGS) -lssl -ltre -lcrypto
