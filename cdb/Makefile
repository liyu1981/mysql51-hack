cdb_prefix=/data/yuli/mysql-dev/mysql-5.1.54/sql/
OBJEXT=o
include ../sql/cdb.mk

mysql_prefix=$(shell grep "^prefix = " ../Makefile | awk '{print $$3}')
prefix=$(mysql_prefix)/bin/

all_exe=cdb_shm_dump

all: $(all_exe)

install:
	install -m 0755 -t $(prefix) $(all_exe)

cdb_shm_dump: cdb_shm_dump.o $(cdb_OBJECTS)
	g++ $(LDFLAG) -o $@ $< $(cdb_OBJECTS)

.cpp.o:
	g++ -c -g $(CXXFLAG) -o $@ $<
