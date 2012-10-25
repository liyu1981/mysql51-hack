/* vim: set ts=4 sw=4 tw=0: */
#ifndef _CDB_H
#define _CDB_H

#include <string.h>
#include <sys/time.h>

extern int cdb_errno;
extern int cdb_2nd_errno;

///////////////////////////////////////////////////////////////////////////////

// for mysqld
bool cdb_init_shm_mgr(const char* mysqld_data_path);
bool cdb_shutdown_shm_mgr();
int cdb_flush_shm_pair_map();

// for cdb tools
bool cdb_attach_shm_mgr(const char* mysqld_data_path);

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

///////////////////////////////////////////////////////////////////////////////
// section: instance level connection statistic

#pragma pack(push, 1)
struct CDBInsConnKey
{
    int _ip;
    int _result;
};

struct CDBInsConn
{
    CDBInsConnKey _key;
    CDBCommStat _comm_stat;
};
#pragma pack(pop)

void cdb_ins_conn_add(CDBInsConn& conn, unsigned long long int begin_time, unsigned long long end_time);

inline void cdb_ins_conn_add(int ip, int result, unsigned long long int begin_time, unsigned long long end_time)
{
    CDBInsConn c;
    c._key._ip = ip;
    c._key._result = result;
    cdb_ins_conn_add(c, begin_time, end_time);
}

#define CDB_INS_CONN_ERROR_ACCEPT                                    (int)10001
#define CDB_INS_CONN_ERROR_NEW_SOCK                                  (int)10002
#define CDB_INS_CONN_ERROR_NEW_THD                                   (int)10003
#define CDB_INS_CONN_ERROR_NEW_VIO                                   (int)10004
#define CDB_INS_CONN_ERROR_TOO_MANY_CONN                             (int)10005
#define CDB_INS_CONN_ERROR_OUT_OF_RES                                (int)10006

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// section: instance level dml per client statistic

#pragma pack(push, 1)
struct CDBInsClientDmlKey
{
    int _ip;
    int _type;
};

struct CDBInsClientDml
{
    CDBInsClientDmlKey _key;
    CDBCommStat _comm_stat;
};
#pragma pack(pop)

void cdb_ins_client_dml_add(CDBInsClientDml& op, unsigned long long int begin_time, unsigned long long int end_time);

///////////////////////////////////////////////////////////////////////////////

#endif

