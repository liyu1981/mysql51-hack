#ifndef _CDB_H
#define _CDB_H

#include <string.h>
#include <sys/time.h>

extern int cdb_errno;
extern int cdb_2nd_errno;

///////////////////////////////////////////////////////////////////////////////

// for mysqld
bool init_cdb_shm_mgr(const char* mysqld_data_path);
bool shutdown_cdb_shm_mgr();
int cdb_flush_shm_pair_map();

// for cdb tools
bool attach_cdb_shm_mgr(const char* mysqld_data_path);

///////////////////////////////////////////////////////////////////////////////
// section: instance level dml statistic

#define CDB_TIME_BUCKET_SIZE 9

#define CDB_UNKOWN_TYPE 0
#define CDB_SELECT 1
#define CDB_INSERT 2
#define CDB_UPDATE 3
#define CDB_REPLACE 4
#define CDB_DELETE 5

#pragma pack(push, 1)
struct CDBCommStat
{
    int _total;
    double _time_sum;
    double _time_min;
    double _time_max;
    int _time_bucket[CDB_TIME_BUCKET_SIZE];
};

struct CDBInsDmlOpKey
{
    int _type;
    int _result;
};

struct CDBInsDmlOp
{
    CDBInsDmlOpKey _key;
    CDBCommStat _comm_stat;
};

#pragma pack(pop)

void cdb_comm_stat_add(CDBCommStat& cs, double v, bool init = false);
void cdb_ins_dml_op_add(CDBInsDmlOp& op, unsigned long long int begin_time, unsigned long long int end_time);

///////////////////////////////////////////////////////////////////////////////

#endif

