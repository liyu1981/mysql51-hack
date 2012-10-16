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
#include "tfc_spin_lock.h"

#define CDB_SHM_LOCKS_NAME "cdb_shm_locks"

namespace cdb {

using namespace std;
using namespace tfc;
using namespace tfc::cache;

struct CDBShm
{
    string       _name;
    key_t        _key;
    size_t       _size;
    int          _id;
    void*        _addr;
    bool         _new;
    CacheAccess* _ca;
    spinlock_t*  _lock;
};

class CDBShmMgr
{
public:
    static CDBShmMgr& getInstance();
    int init(const char* mysqld_data_path, int shm_conf_size, ofstream& shmid_of);
    int attach(const char* mysqld_data_path, int shm_conf_size);
    CDBShm& get(const char* c);
    bool reg(const CDBShm& s);

    CDBShm& get(const string& n) { return get(n.c_str()); }

    bool reg(const char* n, key_t k, size_t s)
    {
        CDBShm cs;
        cs._name = n;
        cs._key = k;
        cs._size = s;
        return reg(cs);
    }

    void detach_all();

private:
    static CDBShmMgr* _instance;
    map<string, CDBShm*> _shm_map;

private:
    CDBShmMgr();
    CDBShmMgr(const CDBShmMgr&);
    CDBShmMgr& operator=(const CDBShmMgr&);
};

} // end of namespace cdb

#endif

