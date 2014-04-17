CPPFLAGS=-I.. -I../.. -I. -I/usr/local/include -I/opt/local/include -L/usr/local/lib  -L/opt/local/lib
all:
	(cd ..; $(MAKE))


stand: feature_sql
feature_sql: feature_recorder_sql.cpp beregex.cpp
	g++ -o stand feature_recorder_sql.cpp beregex.cpp histogram.cpp unicode_escape.cpp -DSTAND -lsqlite3 $(CPPFLAGS) -lssl -ltre -lcrypto
