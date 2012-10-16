cdb_prefix=$(shell readlink -f ..)/sql/
LIBCDB_INC=../cdb
LIBCDB_A=../cdb/libcdb.a

mysql_prefix=$(shell grep "^prefix = " ../Makefile | awk '{print $$3}')
prefix=$(mysql_prefix)/bin/

all_exe=cdb_shm_dump

all: $(all_exe)

install:
	install -m 0755 -t $(prefix) $(all_exe)

cdb_shm_dump: cdb_shm_dump.o
	g++ $(LDFLAG) -o $@ $< $(LIBCDB_A)

.cpp.o:
	g++ -c -g $(CXXFLAG) -I$(LIBCDB_INC) -o $@ $<

clean:
	rm *.o
	rm $(all_exe)
