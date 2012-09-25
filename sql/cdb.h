#ifndef _CDB_H
#define _CDB_H

#include <string.h>
#include <sys/time.h>

struct CDBShmConf
{
    const char* _name;
    int _id;
    size_t _size;
    int _node_total;
    int _bucket_size;
    int _n_chunks;
    int _chunk_size;
};

extern CDBShmConf cdb_shm_conf_array[];
extern int cdb_shm_conf_size;

extern int cdb_errno;
extern int cdb_2nd_errno;

bool init_cdb_shm_mgr(const char* mysqld_data_path);

#define CDB_SELECT 1

#pragma pack(push, 1)
struct CDBInsDmlOpKey
{
    int _type;
    int _result;
};

struct CDBInsDmlOp
{
    CDBInsDmlOpKey _key;
    int _total;
    double _time_sum;
    double _time_min;
    double _time_max;
};

struct CDBInsDmlOpJunk
{
    struct timeval _begin_tval;
    struct timeval _end_tval;
};
#pragma pack(pop)

void cdb_ins_dml_begin(CDBInsDmlOp& op, CDBInsDmlOpJunk& op_junk);
void cdb_ins_dml_end(CDBInsDmlOp& op, CDBInsDmlOpJunk& op_junk);

#endif

