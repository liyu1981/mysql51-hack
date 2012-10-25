cdb_prefix=$(shell readlink -f ..)/sql/
LIBCDB_INC=../cdb
LIBCDB_A=../cdb/libcdb.a -lpthread

mysql_prefix=$(shell grep "^prefix = " ../Makefile | awk '{print $$3}')
prefix=$(mysql_prefix)/bin/

exe_comm_obj=cdb_tool_comm.o

all_exe=cdb_shm_dump cdb_shm_conf_calc
all_script=cdb_ins_dml_monitor cdb_ins_conn_monitor cdb_ins_client_dml_monitor

all: $(all_exe)

install:
	install -m 0755 -t $(prefix) $(all_exe)
	install -m 0755 -t $(prefix) $(all_script)

cdb_shm_dump: cdb_shm_dump.o $(exe_comm_obj)
	g++ $(LDFLAG) -o $@ $^ $(LIBCDB_A)

cdb_shm_conf_calc: cdb_shm_conf_calc.o $(exe_comm_obj)
	g++ $(LDFLAG) -o $@ $^ $(LIBCDB_A)

.cpp.o:
	g++ -c -g $(CXXFLAG) -I$(LIBCDB_INC) -o $@ $<

clean:
	rm *.o
	rm $(all_exe)
