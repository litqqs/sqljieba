# Where MySQL headers are installed
MYSQL_PATH = /usr/include/mysql
# Where MySQL libraries are installed
MYSQL_LIB_PATH = /usr/lib/mysql

CPPFLAGS = -DMYSQL_DYNAMIC_PLUGIN -DMYSQL_ABI_CHECK -DLOGGING_LEVEL=LL_WARNING
CPPFLAGS += -I./deps/ -I$(MYSQL_PATH)

MARIADB_H_PATH=/C/apps/mariadb/include/mysql/


libsqljieba.so: sqljieba.cpp
	g++ -O2 -fPIC -o sqljieba.o $(CPPFLAGS) -c sqljieba.cpp
	g++ -shared -fPIC -o libsqljieba.so sqljieba.o

sqljieba.dll: sqljieba.cpp
	clang++ -o sqljieba.dll -c sqljieba.cpp  -shared -O2 -fPIC  -DMYSQL_DYNAMIC_PLUGIN -DMYSQL_ABI_CHECK -DLOGGING_LEVEL=LL_WARNING -I./deps/ -I$(MARIADB_H_PATH)server/ -I$(MARIADB_H_PATH)server/mysql/ 

clean:
	rm -f *.so *.o *.dll

install:
	cp libsqljieba.so $(MYSQL_LIB_PATH)/plugin/
	cp -r ./dict /usr/share/
