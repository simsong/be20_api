CPPFLAGS=-I.. -I../..  
all:
	(cd ../make)


feature_sql: feature_sql.cpp
	g++ -o feature_sql feature_sql.cpp -DSTAND -lsqlite3 $(CPPFLAGS)
