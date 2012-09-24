#ifndef _CDB_H
#define _CDB_H

#include <string.h>

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

#endif

