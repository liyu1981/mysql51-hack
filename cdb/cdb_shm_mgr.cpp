#include "cdb_shm_mgr.h"
#include "cdb_error.h"
#include "tfc_spin_lock.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

namespace cdb {

static const int C_DEFAULT_OPEN_FLAG = 0600;

CDBShmMgr* CDBShmMgr::_instance = NULL;

CDBShm wrong_shm = {"", 0, 0, 0, 0, false, 0, 0};

CDBShmMgr::CDBShmMgr()
{
}

CDBShmMgr&
CDBShmMgr::getInstance()
{
    if (NULL == _instance) {
        _instance = new CDBShmMgr();
    }
    return *_instance;
}

int
CDBShmMgr::init(const char* mysqld_data_path, int shm_conf_size, ofstream& shmid_of)
{
    size_t size = sizeof(spinlock_t)*shm_conf_size;
    key_t key = ftok(mysqld_data_path, 0);
    if (!reg(CDB_SHM_LOCKS_NAME, key, size)) {
        return CDB_SHM_INIT_SHMLOCKS_ERROR;
    }
    CDBShm& sm = get(CDB_SHM_LOCKS_NAME);
    if (!sm._new) {
        return CDB_SHM_INIT_SHMLOCKS_STALE;
    }
    shmid_of << CDB_SHM_LOCKS_NAME << " " << key << endl;
    return CDB_FINE;
}

int
CDBShmMgr::attach(const char* mysqld_data_path, int shm_conf_size)
{
    size_t size = sizeof(spinlock_t)*shm_conf_size;
    key_t key = ftok(mysqld_data_path, 0);
    if (!reg(CDB_SHM_LOCKS_NAME, key, size)) {
        return CDB_SHM_INIT_SHMLOCKS_ERROR;
    }
    return CDB_FINE;
}

CDBShm&
CDBShmMgr::get(const char* c)
{
    string s(c);
    map<string, CDBShm*>::iterator it = _shm_map.find(s);
    if (it == _shm_map.end())
        return wrong_shm;
    return *it->second;
}

bool
CDBShmMgr::reg(const CDBShm& s)
{
    char* p = NULL;

    CDBShm* ns = new CDBShm();
    ns->_name = s._name;
    ns->_key = s._key;
    ns->_size = s._size;
    ns->_id = -1;
    ns->_addr = 0;
    ns->_new = false;

    int id = shmget(ns->_key, ns->_size, C_DEFAULT_OPEN_FLAG);
    if (id < 0) {
        id = shmget(ns->_key, ns->_size, C_DEFAULT_OPEN_FLAG | IPC_CREAT | IPC_EXCL);
        ns->_new = true;
        if (id < 0)
            goto error_return;
    }

    ns->_id = id;
    p = (char*) shmat(ns->_id, NULL, 0);
    if (ssize_t(p) == -1)
        goto error_return;
    ns->_addr = (void*)p;

    if (ns->_new) {
        memset(ns->_addr, 0, ns->_size);
    }

    _shm_map[ns->_name] = ns;

    return true;

error_return:
    delete ns;
    return false;
}

void
CDBShmMgr::detach_all()
{
    for (map<string, CDBShm*>::iterator it = _shm_map.begin(); it != _shm_map.end(); ++it) {
        shmdt(it->second->_addr);
    }
}

void
CDBShmPair::switch_shm()
{
    // 1st: reset standby. standby is readonly, no need to lock for reset
    memset(_standby->_addr, 0, _standby->_size);
    delete _standby->_ca;
    _standby->_ca = new CacheAccess();
    int ret = _standby->_ca->open((char*)_standby->_addr, _standby->_size, true,
                                  _shm_conf->_node_total, _shm_conf->_bucket_size, _shm_conf->_n_chunks, _shm_conf->_chunk_size);
    if (ret != 0) {
        // TOFIX: Fatal error, must exit!
    }

    // 2nd: lock _current and _standby, wait for outstand write
    spin_lock(_current->_lock);
    spin_lock(_standby->_lock);

    // 3rd: switch pointers
    CDBShm* tmp = _current;
    _current = _standby;
    _standby = tmp;

    // 4th: release locks
    spin_unlock(_standby->_lock);
    spin_unlock(_current->_lock);
}

CDBShm&
CDBShmPair::get_current()
{
    return *_current;
}

int
CDBShmPair::flush_map_file(const string& dir_path)
{
    string pair_map_file_path = dir_path + string("/") + string(_pair_conf->_map_file);
    ofstream of;
    of.open(pair_map_file_path.c_str(), std::ios::out | std::ios::trunc);
    if (!of.is_open()) {
        return CDB_SHM_PAIR_MAP_OPEN_FAILED;
    }

    of << "last_switch_time: " << _last_switch_time.tv_sec << " " << _last_switch_time.tv_usec << " "
       << "current: " << _current->_name << " " << _current->_key << " "
       << "standby: " << _standby->_name << " " << _standby->_key << " "
       << endl;

    of.close();
    return CDB_FINE;
}

} // end of namespace cdb


