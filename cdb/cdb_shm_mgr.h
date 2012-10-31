/* vim: set ts=4 sw=4 tw=0: */
#ifndef _CDB_SHM_MGR_H_
#define _CDB_SHM_MGR_H_

#include <string>
#include <map>
#include <fstream>
#include <sys/shm.h>
#include <unistd.h>

#include "cdb_error.h"
#include "tfc_shm_map.h"
#include "tfc_cache_access.h"

namespace cdb {

using namespace std;
using namespace tfc;
using namespace tfc::cache;

const int CDB_SHM_META_MAX = 4096;

/* CDBShm layout:

   without meta info case:
    __________ _size bytes ___________________
   /                                          \
   --------------------------------------------
   | _data_size bytes
   --------------------------------------------
   ^
   |
   _addr
   _data_addr
   (_meta_addr = 0)

   with meta info case:
    __________ _size bytes ___________________
   /                                          \
   --------------------------------------------
   | META_MAX | _data_size bytes
   --------------------------------------------
   ^          ^
   |          |
   _addr      _data_addr
   _meta_addr

 */
struct CDBShm
{
    string           _name;
    key_t            _key;
    size_t           _size;
    int              _id;
    void*            _addr;
    bool             _new;
    CacheAccess*     _ca;
    pthread_mutex_t* _lock;
    void*            _data_addr;
    size_t           _data_size;
    void*            _meta_addr;
};

class CDBShmMgr
{
public:
    static CDBShmMgr& getInstance();
    int init(const char* mysqld_data_path, int shm_conf_size, ofstream& shmid_of);
    int attach(const char* mysqld_data_path, int shm_conf_size);
    CDBShm& get(const char* c);
    bool reg(const CDBShm& s, bool has_meta = true);

    CDBShm& get(const string& n) { return get(n.c_str()); }

    bool reg(const char* n, key_t k, size_t s, bool has_meta = true)
    {
        CDBShm cs;
        cs._name = n;
        cs._key = k;
        cs._size = s;
        return reg(cs, has_meta);
    }

    void detach_all();

    pthread_mutex_t* get_lock(int i) { return &_shm_lock_array[i]; }

    void destroy_all_lock()
    {
        for (int i=0; i<_shm_lock_array_size; ++i) {
            pthread_mutex_destroy(&_shm_lock_array[i]);
        }
    }

private:
    static CDBShmMgr* _instance;

    map<string, CDBShm*> _shm_map;
    pthread_mutex_t*     _shm_lock_array;
    int                  _shm_lock_array_size;

private:
    CDBShmMgr();
    CDBShmMgr(const CDBShmMgr&);
    CDBShmMgr& operator=(const CDBShmMgr&);
};

struct CDBShmConf
{
    string _name;
    int _id;
    size_t _size;
    int _node_total;
    int _bucket_size;
    int _n_chunks;
    int _chunk_size;
};

struct CDBShmPairConf
{
    string _name;
    string _shm_name1;
    string _shm_name2;
    int    _conf_index;
    string _map_file;
};

struct CDBShmPair
{
    struct timeval        _last_switch_time;
    struct CDBShm*        _current;
    struct CDBShm*        _standby;
    const CDBShmConf*     _shm_conf;
    const CDBShmPairConf* _pair_conf;

    CDBShm& get_current();
    void switch_shm();
    int flush_pair_info(const string& dir_path);

    int flush_pair_meta_data(const string& dir_path);
    int flush_pair_map_file(const string& dir_path);
};

} // end of namespace cdb

#endif

